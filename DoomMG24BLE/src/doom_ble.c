/***************************************************************************//**
 * @file doom_ble.c
 * @brief BLE related functions for Doom on xG24 series devices.
 * @note: oh man, this needs really a clean up.
 *
 *  Copyright (C) 2022-2023 by Nicola Wrachien
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 * */
#include "em_common.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#include "main.h"
#include "em_device.h"
#include "doom_ble.h"
#include "em_cmu.h"
#include "em_timer.h"
#include "sl_system_process_action.h"
#include "z_zone.h"
#include "global_data.h"
#include "i_system.h"
#include "delay.h"
#define ENABLE_BLE_DEBUG 0
#if ENABLE_BLE_DEBUG
  #include "printf.h"
  #warning ENABLE_BLE_DEBUG is non-zero. This might disrupt correct operation!
#define dbg_printf          printf
#else
  #define dbg_printf(...)
  #define app_assert_status(c) (void)c
#endif
#define SCAN_INTERVAL                   1000
#define MIN_ADV_INTERVAL_HOST           80
#define MAX_ADV_INTERVAL_HOST           90
#define MIN_ADV_INTERVAL_CLIENT         90
#define MAX_ADV_INTERVAL_CLIENT         100
#define MAX_CONNECTION_TIME             4000000U
#define SUPERVISOR_TIMEOUT              400     // time = SUPERVISOR_TIMEOUT x 10ms
#define MIN_CONN_INTERVAL               6  //
#define MAX_CONN_INTERVAL               6 //
#if HAS_NETWORK
#define MAX_CLIENT_DATA_SKIP_TIME      250  // TBD -- 100?
static uint32_t rndId;                      // game ID
static uint8_t gameMode;
static uint8_t clientStatus = BLE_CLIENT_DISCONNECTED;
char localPlayerName[MAX_HOST_NAME_LENGTH] = {'D','O','O','M','G','U','Y'};
static char serverPlayerName[MAX_HOST_NAME_LENGTH];   // this is the server name. It is always player 0
hostData_t * pHostData = NULL;
static uint8_t mustStartGame = 0;
uint8_t peripheral_connection_handle;
multiplayerGameSettings_t hostMultiplayerGameSettings;
static unsigned int checkNumber = 0;
typedef struct
{
    uint8_t lenFlags;
    uint8_t typeFlags;
    uint8_t flags;
    uint8_t len128uuid;
    uint8_t type128uuid;
    uint8_t uuid128[16];
    uint8_t lenShortName;
    uint8_t typeShortName;
    uint8_t shortName[8];
} bleDoomHostAdv_t;
typedef struct
{
    uint8_t lenMfgSpecific;
    uint8_t typeMfgSpecific;
    uint16_t mfgId;
    multiplayerGameSettings_t settings;
}__attribute__ ((packed)) bleDoomHostScan_t;
typedef struct
{
    uint8_t lenFlags;
    uint8_t typeFlags;
    uint8_t flags;
    uint8_t len128uuid;
    uint8_t type128uuid;
    uint8_t uuid128[16];
    uint8_t lenMfgSpecific;
    uint8_t typeMfgSpecific;
    uint16_t mfgId;
    uint32_t rndId;
} __attribute__ ((packed)) bleDoomClientAdv_t;
typedef struct
{
    uint8_t lenShortName;
    uint8_t typeShortName;
    uint8_t shortName[8];
    uint8_t lenMfgSpecific;
    uint8_t typeMfgSpecific;
    uint16_t mfgId;
    uint8_t clientName[8];
} __attribute__ ((packed)) bleDoomClientScan_t;

typedef struct {
    bd_addr device_address;
    uint8_t connection_handle;
    uint8_t connection_state;
    uint8_t remoteName[8];
} bleDoomClient_t;
typedef struct
{
    uint8_t cmd;
    uint8_t connectedMask;
    uint8_t localPlayerNumber;
    uint8_t serverPlayerName[MAX_HOST_NAME_LENGTH];
    uint8_t clientPlayerNames[MAX_CLIENTS][MAX_HOST_NAME_LENGTH];
} playersNameCmd_t;
bleDoomClient_t bleDoomClients[MAX_CLIENTS];
// Generate an error if bleDoomHostAdv_t is larger than 31
char bledummy0[ sizeof(bleDoomHostAdv_t) <= 31 ? 0 : - 1];
// can multiplayerGameSettings_t fit into the scan response?
char bledummy1[sizeof(multiplayerGameSettings_t) + 4 <= 31 ? 0 : -1];
// check size of bleDoomClientAdv_t
char bledummy2[sizeof(bleDoomClientAdv_t) <= 31 ? 0 : -1];
// check size of bleDoomHostScan_t
char bledummy3[sizeof(bleDoomHostScan_t) <= 31 ? 0 : -1];
uint8_t bleMustStartGame(void)
{
  return mustStartGame;
}
void bleResetMustStartGame(void)
{
  mustStartGame = 0;
}
// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

uint8_t bleGetClientStatus(void)
{
  return clientStatus;
}
static void bleAddHostData(uint8_t *address, uint8array *data, uint32_t type)
{
  if (NULL == pHostData)
  {
    return;     // we should assert there but nevermind
  }
  // get the time, to check which is the slot that was older
  uint32_t timeNow = I_GetTimeMicrosecs();
  uint32_t maxTime = 0;
  uint32_t maxTimeIndex = 0;
  int i;
  for (i = 0; i < NET_MAX_HOST_LIST; i++)
  {
    // calculate how many microseconds ago the host was last seen
    uint32_t time = timeNow - pHostData[i].lastSeen;
    // if the time is longer than the previous, then update the index and time
    if (time > maxTime)
    {
      maxTime = time;
      maxTimeIndex = i;
    }
    // do we have already this address?
    if (0 == memcmp((void*) pHostData[i].bleAddr, address, sizeof(pHostData[i].bleAddr)))
    {
      // found!
      break;
    }
  }
  //
  if (i < NET_MAX_HOST_LIST) // found!
  {
    // update last seen
    pHostData[i].lastSeen = timeNow;
    //
  }
  else  if (!(type & SL_BT_SCANNER_EVENT_FLAG_SCAN_RESPONSE))
  {
    // not found. if this is an advertising data then replace the oldest entry
    bleDoomHostAdv_t *advData = (bleDoomHostAdv_t *) &data->data;
    if (0 == memcmp(advData->uuid128, static_gattdb->attributes[gattdb_doom_host - 1].constdata->data, sizeof(advData->uuid128)))
    {
      // replace the old one
      i = maxTimeIndex;
      pHostData[i].lastSeen = timeNow;
      memcpy((void*) pHostData[i].bleAddr, address, sizeof(pHostData[i].bleAddr));
      // not valid until we get the scan result data
      pHostData[i].settings.valid = 0;
    }
  }
  // Got scan data, and found a free list?
  if ((type & SL_BT_SCANNER_EVENT_FLAG_SCAN_RESPONSE) && i < NET_MAX_HOST_LIST)
  {
    // ERROR THIS MUST USE AN OFFSET AS SETTINGS ARE NOT IN THE EXACT POSITION. BETTER USE A TYPEDEF
    memcpy((void*) &pHostData[i].settings, &((bleDoomHostScan_t * )(data->data))->settings, sizeof(pHostData[i].settings));
    // set as valid.
// TODO: check mfg specific etc.
    pHostData[i].settings.valid = 1;
  }
}
//
enum
{
  CLIENT_CONNECTION_WAIT_ADV_DATA,
  CLIENT_CONNECTION_WAIT_SCAN_DATA,
  CLIENT_CONNECTION_CONNECT,
  CLIENT_CONNECTION_FULL,
};
static uint8_t clientConnectionState = CLIENT_CONNECTION_WAIT_ADV_DATA;
static bd_addr currClientAddr;
static uint8_t currClientName[8];
static uint8_t conn_h;
static void bleAddClientData(uint8_t *address, uint8array *data, uint32_t type, uint32_t addrType)
{
  // this variable holds the last time in which the host was not in CLIENT_CONNECTION_CONNECT
  static uint32_t lastNotConnectTime = 0;
  if (clientConnectionState == CLIENT_CONNECTION_CONNECT)
  {
    if (I_GetTimeMicrosecs() - lastNotConnectTime > MAX_CONNECTION_TIME) // trying connecting in more than 2 s?
    {
      clientConnectionState = CLIENT_CONNECTION_WAIT_ADV_DATA;
      lastNotConnectTime = I_GetTimeMicrosecs();
    }
    else
      return;
  }
  else
  {
    lastNotConnectTime = I_GetTimeMicrosecs();
  }
  if (clientConnectionState == CLIENT_CONNECTION_FULL)  // should not happen
    return;
  // is an adv data?
  if (!(type & SL_BT_SCANNER_EVENT_FLAG_SCAN_RESPONSE))
  {
    bleDoomClientAdv_t *advData = (bleDoomClientAdv_t *) &data->data;
    // is this what we are looking for? (same rndId and right uuid?)
    if (0 == memcmp(advData->uuid128, static_gattdb->attributes[gattdb_doom_client - 1].constdata->data, sizeof(advData->uuid128)) && advData->rndId == rndId)
    {
        memcpy((void*)&currClientAddr, address, sizeof(bd_addr));
        clientConnectionState = CLIENT_CONNECTION_WAIT_SCAN_DATA; // we are interested in what the sccan answer will be
    }
  }
  else if (clientConnectionState ==  CLIENT_CONNECTION_WAIT_SCAN_DATA && 0 == memcmp(address, (void*) &currClientAddr, sizeof(bd_addr)))
  {
    clientConnectionState = CLIENT_CONNECTION_CONNECT;
    bleDoomClientScan_t *scanData = (bleDoomClientScan_t *) &data->data;
    // let's copy the name
    memcpy((void*) currClientName, scanData->clientName, sizeof(currClientName));
    sl_status_t sc;
    // Connect to the device
    sc = sl_bt_connection_open(currClientAddr, addrType, sl_bt_gap_phy_1m, (uint8_t*) &conn_h);
    app_assert_status(sc);
  }
}
uint8_t bleHostSendTicsToClient(int clientNumber, int minTicMadeByAll, int minReceivedByAll)
{
  checkNumber++;
  sl_status_t sc = 0;
  bleDoomOtherPlayerTics_t srt;
  int newTics = minTicMadeByAll - minReceivedByAll;
  if (newTics < 0)
  {
    return 0;
  }
  srt.numberOfNewTics = newTics;
  srt.numberOfTicsReceivedByAll = minReceivedByAll;
  srt.connMask = 1 | (_g->playeringame[1] << 1) | (_g->playeringame[2] << 2) | (_g->playeringame[3] << 3);
  srt.checkNumber = checkNumber;
  srt.checkNumber2 = ~checkNumber;

  bleDoomClient_t * cl = (bleDoomClient_t *) &bleDoomClients[clientNumber];
  if (cl->connection_state)
  {
    // fill out srt
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
      int p = i + 1;  // player is clientNumber + 1 (host is 0)
      if (i == clientNumber)
      {
        p = 0;          // however we won't send the client data to client! We are sending host data!
      }
      srt.playerNumber[i] = p;
      if (newTics > BACKUPTICS)
      {
        printf("Error! newTics > BACKUPTICKS\r\n");
        newTics = BACKUPTICS;
      }
      for (int t = 0; t < newTics; t++)
      {
        srt.ticcmds[i][t] = _g->netcmds[p][(minReceivedByAll + t) % BACKUPTICS];
      }
    }
    sc = sl_bt_gatt_write_characteristic_value(cl->connection_handle,
                                                                             gattdb_otherNodeTics,
                                                                             sizeof(srt),
                                                                             (uint8_t*) &srt);
  }
  return sc;
}
uint8_t bleUpdateTicsToServer(int numberOfTics)
{
  checkNumber++;
  const int id[MAX_CLIENTS] = {gattdb_clientTics1, gattdb_clientTics2, gattdb_clientTics3};
  int playerNumber = _g->consoleplayer;
  uint16_t sentlen;
  bleDoomClientTics_t clt;
  static int oldremotetic = 0;
  static int oldNumberOfTics = 0;
  static uint8_t skipped = MAX_CLIENT_DATA_SKIP_TIME; // since we are sending value without response, the server might miss our data. It's better if after some attempt we don't have any change, we try to resend anyway.
  if (_g->remotetic == oldremotetic && oldNumberOfTics == numberOfTics)
  {
    skipped++;
    if (skipped < MAX_CLIENT_DATA_SKIP_TIME)
    {
      return 0;
    }
  }
  skipped = 0;
  clt.numberOfReceivedTicsByClient = _g->remotetic;
  clt.numberOfTiccmds = numberOfTics;
  clt.checkNumber = checkNumber;
  clt.checkNumber2 = ~checkNumber;
  oldNumberOfTics = numberOfTics;
  oldremotetic = _g->remotetic;
  if (numberOfTics > BACKUPTICS)
  {
    printf("Error numberOfTics > BACKUPTICS\r\n");
    numberOfTics = BACKUPTICS;
  }
  for (int i = 0; i < numberOfTics; i++)
  {
    // fill data
    clt.ticcmds[i] = _g->netcmds[playerNumber][(_g->remotetic + i) % BACKUPTICS];
  }
  // send
  sl_status_t sc = sl_bt_gatt_write_characteristic_value_without_response(peripheral_connection_handle,
                                                              id[_g->consoleplayer - 1],
                                                              sizeof(clt),
                                                              (uint8_t*) &clt,
                                                              &sentlen);
  return sc;

}
uint8_t bleReadOtherPlayerTics(bleDoomOtherPlayerTics_t * otherPlayerTics)
{
  sl_status_t sc;
  size_t readlen;
  sc =  sl_bt_gatt_server_read_attribute_value(gattdb_otherNodeTics, 0, sizeof (bleDoomOtherPlayerTics_t) , &readlen, (uint8_t*) otherPlayerTics);
  if (sc)
  {
    printf("bleReadOtherPlayerTics sc %x\r\n",sc);
    memset(otherPlayerTics, 0, sizeof(*otherPlayerTics));
    return 2;
  }
  if (readlen != sizeof(bleDoomOtherPlayerTics_t))
  {
    return 1;
  }
  else
  {
    if (otherPlayerTics->checkNumber == ~otherPlayerTics->checkNumber2)
    {
      for (int i = 0; i < MAXPLAYERS; i++)
      {
        _g->playeringame[i] = (otherPlayerTics->connMask & (1 << i)) ? 1 : 0;
      }
      return 0;
    }
    else
    {
      printf("Error checknumber %d not ~%d\r\n", otherPlayerTics->checkNumber, otherPlayerTics->checkNumber2);
      return 3;
    }
  }
}
uint8_t bleReadClientTics(bleDoomClientTics_t *cmd, int clientNumber)
{
  sl_status_t sc;
  const int id[MAX_CLIENTS] = {gattdb_clientTics1, gattdb_clientTics2, gattdb_clientTics3};
  size_t readlen;
  sc = sl_bt_gatt_server_read_attribute_value(id[clientNumber], 0, sizeof (bleDoomClientTics_t) , &readlen, (uint8_t*) cmd);
  if (sc)
  {
    printf("Sc: %x\r\n", sc);
    return 2;
  }
  if (readlen != sizeof(bleDoomClientTics_t))
  {
    printf("ReadLen: %d\r\n", readlen);
    return 1;
  }
  else
  {
    // len correct, what about check number?
    if (cmd->checkNumber == ~cmd->checkNumber2)
      return 0;
    else
    {
      printf("Error checknumber %d %d\r\n", cmd->checkNumber, cmd->checkNumber2);
      return 3;
    }
  }
}

// This function gets client data in host mode.
static uint8_t bleGetClientData(uint8_t i, uint8_t *name, uint8_t *addr, uint8_t *handle)
{
  if (i >= MAX_CLIENTS || bleDoomClients[i].connection_state == 0)
    return BLE_PLAYER_NOT_CONNECTED;
  memcpy(name, (void*) bleDoomClients[i].remoteName, sizeof(bleDoomClients[i].remoteName));
  memcpy(addr, (void*) &bleDoomClients[i].device_address, sizeof(bleDoomClients[i].device_address));
  if (handle)
  {
    *handle = bleDoomClients[i].connection_handle;
  }
  return BLE_PLAYER_IS_REMOTE_HOST;
}
/**
 * Counts how many clients are connected
 * @return number of clients connected
 */
int bleCountClients(void)
{
  int n = 0;
  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (bleDoomClients[i].connection_state)
    {
      n++;
    }
  }
  return n;
}
// Ask clients to start the game.
void   bleRequestClientsStartGame(void)
{
  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (bleDoomClients[i].connection_state)
    {
      uint8_t cmd = BLE_DOOM_CMD_START_LEVEL;
      sl_bt_gatt_write_characteristic_value(bleDoomClients[i].connection_handle, gattdb_doom_client_cmd , 1, &cmd);
    }
  }
}
// update player lists to connected clients.
void bleUpdatePlayers(void)
{
  uint8_t connMask = 1;
  playersNameCmd_t pn;
  hostMultiplayerGameSettings.clients = 0;
  // create  connected State mask
  pn.cmd = BLE_DOOM_CMD_SET_PLAYER_INFO;
  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (bleDoomClients[i].connection_state)
    {
      connMask |= 1 << (i + 1);
      // update player is in game
      _g->playeringame[i + 1] = true;
      hostMultiplayerGameSettings.clients++;
    }
    else
    {
      _g->playeringame[i + 1] = false;
    }
    memcpy(pn.clientPlayerNames[i], (void*)bleDoomClients[i].remoteName, MAX_HOST_NAME_LENGTH);
  }
  pn.connectedMask = connMask;
  // copy names
  memcpy(pn.serverPlayerName, localPlayerName, MAX_HOST_NAME_LENGTH);
  _g->playeringame[0] = true; // host always connected
  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (bleDoomClients[i].connection_state)
    {
      // player is connected. let's write on its characteristics
      pn.localPlayerNumber = 1 + i;
      // write to characteristics
      sl_bt_gatt_write_characteristic_value(bleDoomClients[i].connection_handle, gattdb_doom_client_cmd , sizeof(pn), (uint8_t *) &pn);
    }
  }
  // Updating adv data only when there are more client slots left
  if (bleCountClients() >= MAX_CLIENTS)
  {
    printf("Stop Scanning and adv, no more slots left\r\n");
    bleStopScanAndAdvertising();
  }
  else if (_g->waitingForClients)
  {
    bleUpdateAdvertisingData(BLE_MODE_HOST, &hostMultiplayerGameSettings);
  }
}
//
uint8_t bleGetGameMode(void)
{
  return gameMode;
}
void bleSetPlayerInfo(playersNameCmd_t *info)
{
  uint8_t mask = info->connectedMask;
  //
  _g->consoleplayer = info->localPlayerNumber;
  _g->displayplayer = _g->consoleplayer;
  // set server player name
  memcpy(serverPlayerName, info->serverPlayerName, MAX_HOST_NAME_LENGTH);
  //
  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    memcpy((void*) bleDoomClients[i].remoteName, info->clientPlayerNames[i], MAX_HOST_NAME_LENGTH);
    bleDoomClients[i].connection_state = 0 != (mask & (1 << (1 + i)));
    if (_g->menuactive)   // this to prevent a race between this and the tics cmd charateristics.
    {
      _g->playeringame[i + 1] = bleDoomClients[i].connection_state;
    }
  }
  _g->playeringame[0] = true;
}

void bleCheckCommand(sl_bt_evt_gatt_server_attribute_value_t *v)
{
  (void) v;
  if (gameMode == BLE_MODE_HOST)
  {
    return;     //
  }
#if 0
  uint8_t *data = v->value.data;
  uint32_t len = v->value.len;
  if (!len)
  {
    return;
  }
  switch (data[0])
  {
    case BLE_DOOM_CMD_START_LEVEL:  //
      mustStartGame = 1;
      break;
    case BLE_DOOM_CMD_SET_PLAYER_INFO:
      bleSetPlayerInfo((playersNameCmd_t*) v->value.data);
      break;

  }
#else
  uint8_t buffer[sizeof(playersNameCmd_t)];
  size_t readlen;
  sl_bt_gatt_server_read_attribute_value(gattdb_doom_client_cmd, 0, sizeof (buffer) , &readlen, buffer);
  switch (buffer[0])
  {
    case BLE_DOOM_CMD_START_LEVEL:  //
         mustStartGame = 1;
         break;
       case BLE_DOOM_CMD_SET_PLAYER_INFO:
         if (readlen >= sizeof(playersNameCmd_t))
         {
           bleSetPlayerInfo((playersNameCmd_t*) buffer);
         }
         break;
  }
#endif
}
uint8_t bleGetPlayerData(uint8_t playerNumber, uint8_t *name, uint8_t *addr, uint8_t *handle)
{
  uint8_t rv = BLE_PLAYER_NOT_CONNECTED;
  if (BLE_MODE_HOST == gameMode)
  {
    if (playerNumber == 0)
    {
      memcpy(name, localPlayerName, MAX_HOST_NAME_LENGTH);
      rv = BLE_PLAYER_IS_LOCAL_HOST;
    }
    else
    {
      rv = bleGetClientData(playerNumber - 1, name, addr, handle);
    }
  }
  else
  {
    if (playerNumber == _g->consoleplayer)
    {
      rv = BLE_PLAYER_IS_LOCAL_HOST;
      memcpy(name, localPlayerName, MAX_HOST_NAME_LENGTH);
    }
    else if (0 == playerNumber)
    {
      rv = BLE_PLAYER_IS_REMOTE_HOST;
      memcpy(name, serverPlayerName, MAX_HOST_NAME_LENGTH);
    }
    else if (playerNumber <= MAX_CLIENTS)
    {
      rv = bleDoomClients[playerNumber - 1].connection_state;
      memcpy(name, (void*) bleDoomClients[playerNumber - 1].remoteName , MAX_HOST_NAME_LENGTH);
    }
  }
  return rv;
}
boolean bleIsClientConnected(uint32_t clientNumber)
{
  if (clientNumber >= MAX_CLIENTS)
    return false;
  return bleDoomClients[clientNumber].connection_state != 0;
}
void bleCloseNetwork(void)
{
  if (bleGetGameMode() == BLE_MODE_HOST)
  {
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
      bleConnectionClose(i);
    }
    bleStopScanAndAdvertising();
  }
  else
  {
    bleConnectionClose(0);    // any number will do in client mode
  }
}
void bleConnectionClose(uint32_t clientNumber)
{
  if (bleGetGameMode() == BLE_MODE_HOST)
  {
    if (bleIsClientConnected(clientNumber))
    {
      sl_bt_connection_close(bleDoomClients[clientNumber].connection_handle);
    }
  }
  else
  {
    if (bleGetClientStatus())
    {
      sl_bt_connection_close(peripheral_connection_handle);
    }
  }
  bleDoomClients[clientNumber].connection_state = 0;
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  bd_addr address;
  uint8_t address_type;
  uint8_t system_id[8];
  //dbg_printf("BLE ID 0x%08x\r\n",SL_BT_MSG_ID(evt->header));
  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      app_assert_status(sc);

      // Pad and reverse unique ID to get System ID.
      system_id[0] = address.addr[5];
      system_id[1] = address.addr[4];
      system_id[2] = address.addr[3];
      system_id[3] = 0xFF;
      system_id[4] = 0xFE;
      system_id[5] = address.addr[2];
      system_id[6] = address.addr[1];
      system_id[7] = address.addr[0];
      uint16_t dummy;
      sl_bt_gatt_set_max_mtu(247, &dummy);
      sc = sl_bt_gatt_server_write_attribute_value(gattdb_system_id,
                                                   0,
                                                   sizeof(system_id),
                                                   system_id);
      app_assert_status(sc);
      // set default connection parameters
      sl_bt_connection_set_default_parameters(MIN_CONN_INTERVAL, MAX_CONN_INTERVAL, 0, SUPERVISOR_TIMEOUT, 0, 0xFFFF);
      // Create an advertising set.
      sc = sl_bt_advertiser_create_set((uint8_t *) &advertising_set_handle);
      app_assert_status(sc);
      break;
    //
    case sl_bt_evt_scanner_legacy_advertisement_report_id:
      if (BLE_MODE_HOST == gameMode)
      {
        //
        dbg_printf("Received scanner result, address %02x%02x%02x%02x%02x%02x flags: %02x\r\n",
               evt->data.evt_scanner_legacy_advertisement_report.address.addr[0],
              evt->data.evt_scanner_legacy_advertisement_report.address.addr[1],
              evt->data.evt_scanner_legacy_advertisement_report.address.addr[2],
              evt->data.evt_scanner_legacy_advertisement_report.address.addr[3],
              evt->data.evt_scanner_legacy_advertisement_report.address.addr[4],
              evt->data.evt_scanner_legacy_advertisement_report.address.addr[5],
              evt->data.evt_scanner_legacy_advertisement_report.event_flags);
        bleAddClientData(evt->data.evt_scanner_legacy_advertisement_report.address.addr,
                       &evt->data.evt_scanner_legacy_advertisement_report.data,
                       evt->data.evt_scanner_legacy_advertisement_report.event_flags,
                       evt->data.evt_scanner_legacy_advertisement_report.address_type);

      }
      else
      {
        bleAddHostData(evt->data.evt_scanner_legacy_advertisement_report.address.addr,
                       &evt->data.evt_scanner_legacy_advertisement_report.data,
                       evt->data.evt_scanner_legacy_advertisement_report.event_flags);
      }
      break;
    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      if (evt->data.evt_connection_opened.master)
      {
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
          if (bleDoomClients[i].connection_state == 0 || 0 == memcmp(&bleDoomClients[i].device_address , &evt->data.evt_connection_opened.address, sizeof(evt->data.evt_connection_opened.address)))
          {
            bleDoomClients[i].device_address = evt->data.evt_connection_opened.address;
            bleDoomClients[i].connection_state = 2; // require settin parameters
            memcpy ((void*)bleDoomClients[i].remoteName, (void*) currClientName, sizeof(bleDoomClients[i].remoteName));
            bleDoomClients[i].connection_handle = evt->data.evt_connection_opened.connection;
            //
            break;
          }
        }
        // let's count how many devices we have, and stop scanning if needed.
        if (bleCountClients() >= MAX_CLIENTS)
        {
          // stop ng. We don't want any more clients
          printf("Full, Stop ng\r\n");
          sl_bt_scanner_stop();
          clientConnectionState = CLIENT_CONNECTION_FULL;
        }
        else
        {
          clientConnectionState = CLIENT_CONNECTION_WAIT_ADV_DATA;
        }
      }
      else
      {
        bleResetMustStartGame();
        clientStatus = BLE_CLIENT_CONNECTED;
        peripheral_connection_handle = evt->data.evt_connection_opened.connection;
      }
      printf("Open %d, nc %d, ccs: %d\r\n", evt->data.evt_connection_opened.connection, bleCountClients(), clientConnectionState);
      break;
    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      if ( BLE_MODE_HOST == gameMode )
      {
        //
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
          // was in the list ?
          if (bleDoomClients[i].connection_state == 1 && bleDoomClients[i].connection_handle == evt->data.evt_connection_closed.connection)
          {
            _g->playeringame[i + 1] = 0;  // Not sure if this might break sync.
            // remove connection
            bleDoomClients[i].connection_state = 0;
            printf("Restarting again\r\n");
            clientConnectionState = CLIENT_CONNECTION_WAIT_ADV_DATA;
            sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m, sl_bt_scanner_discover_generic);
            break;
          }
        }
        bleUpdatePlayers();
      }
      else
      {
        bleResetMustStartGame();
        clientStatus = BLE_CLIENT_DISCONNECTED;
        peripheral_connection_handle = 0;
      }
      printf("Close %d, nc %d, ccs: %d\r\n", evt->data.evt_connection_closed.connection, bleCountClients(), clientConnectionState);
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////
    case sl_bt_evt_gatt_server_attribute_value_id:
      if (evt->data.evt_gatt_server_attribute_value.attribute == gattdb_doom_client_cmd)
      {
        bleCheckCommand(&evt->data.evt_gatt_server_attribute_value);
      }
      break;
    case sl_bt_evt_connection_parameters_id:

      for (int i = 0; i < MAX_CLIENTS; i++)
      {
        if (bleDoomClients[i].connection_state == 2 && evt->data.evt_connection_parameters.connection == bleDoomClients[i].connection_handle)
        {
          bleDoomClients[i].connection_state = 1;
          // 6-12 ok 6-8 ok
          sl_bt_connection_set_parameters(evt->data.evt_connection_parameters.connection, MIN_CONN_INTERVAL, MAX_CONN_INTERVAL, 0, SUPERVISOR_TIMEOUT, 0, 0xFFFF);
          bleUpdatePlayers();
          break;
        }
      }
      break;
    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}
void bleStartScanningForClients(uint32_t gameRndId)
{
  printf("start ng for clients\r\n");
  gameMode = BLE_MODE_HOST;
  rndId = gameRndId;
  sl_bt_scanner_stop();
  sl_status_t sc;
  sc = sl_bt_scanner_set_parameters(sl_bt_scanner_scan_mode_active, SCAN_INTERVAL, SCAN_INTERVAL);
  if (sc)
  {
    printf("Error setting scan params %x\r\n", sc);
  }
  sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m,  sl_bt_scanner_discover_generic);
  if (sc)
  {
    printf("Error starting scan %x\r\n", sc);
  }
}
void bleStartScanningForHost()
{
  printf("Start Scanning for host\r\n");
  gameMode = BLE_MODE_CLIENT;
  sl_bt_scanner_stop();
  sl_bt_scanner_set_parameters(sl_bt_scanner_scan_mode_active, SCAN_INTERVAL, SCAN_INTERVAL);
  sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m, sl_bt_scanner_discover_generic);
}
void bleStopScanAndAdvertising(void)
{
  printf("Stop adv and scan\r\n");
  sl_bt_advertiser_stop(advertising_set_handle);
  sl_bt_scanner_stop();
}
/**
 * @brief This function will update advertizing data based on level etc. It will also turn on/off the advertising, (if for instance multiplayer game was stopped or if there are no more seats)
 * @param
 */
void bleUpdateAdvertisingData(uint8_t mode, multiplayerGameSettings_t *settings)
{
  gameMode = mode;
  printf("Starting advertising\r\n");
  sl_bt_advertiser_stop(advertising_set_handle);//tart(advertising_set_handle, sl_bt_advertiser_connectable_scannable);
  delay(100);
  if (BLE_MODE_HOST == mode)
  {
    memcpy (&hostMultiplayerGameSettings, settings, sizeof(multiplayerGameSettings_t));
    sl_bt_advertiser_set_timing(
            advertising_set_handle,
            MIN_ADV_INTERVAL_HOST, // min. adv. interval (milliseconds * 1.6)
            MAX_ADV_INTERVAL_HOST, // max. adv. interval (milliseconds * 1.6)
            0,   // adv. duration
            0);  // max. num. adv. events
    // FIRST, SET ADV PACKET
    sl_status_t sc;
    bleDoomHostAdv_t adv;
    adv.lenFlags = 2;
    adv.typeFlags = 1;
    adv.flags = 0x06;
    adv.len128uuid = 0x11;
    adv.type128uuid = 0x07;
    memcpy(&adv.uuid128, static_gattdb->attributes[gattdb_doom_host - 1].constdata->data, sizeof(adv.uuid128));
    adv.typeShortName = 0x08;
    const char sn[sizeof(adv.shortName)] = {'D','O','O','M','H','O','S','T'};
    adv.lenShortName = 1 + sizeof(adv.shortName);
    memcpy(&adv.shortName, sn, sizeof(adv.shortName));
    sc = sl_bt_legacy_advertiser_set_data(advertising_set_handle, sl_bt_advertiser_advertising_data_packet,
                                                 sizeof (bleDoomHostAdv_t),
                                                 (uint8_t*) &adv);

    app_assert_status(sc);
    // SECOND, SET SCAN RESPONSE PACKET WITH ADDITIONAL DATA
    bleDoomHostScan_t scanRsp;
    scanRsp.lenMfgSpecific =  3 + sizeof (*settings);
    scanRsp.typeMfgSpecific = 0xFF;  // mfg specific
    scanRsp.mfgId = 0x02FF;       // Silicon Labs
    memcpy (&scanRsp.settings,  settings, sizeof(multiplayerGameSettings_t));
    sc = sl_bt_legacy_advertiser_set_data(advertising_set_handle, sl_bt_advertiser_scan_response_packet,
                                                 sizeof (scanRsp),
                                                 (uint8_t*) &scanRsp);
    app_assert_status(sc);


    sc = sl_bt_legacy_advertiser_start(advertising_set_handle, sl_bt_advertiser_scannable_non_connectable);

    app_assert_status(sc);

  }
  else
  {
    sl_status_t sc;
    sc = sl_bt_advertiser_set_timing(
            advertising_set_handle,
            MIN_ADV_INTERVAL_CLIENT, // min. adv. interval (milliseconds * 1.6)
            MAX_ADV_INTERVAL_CLIENT, // max. adv. interval (milliseconds * 1.6)
            0,   // adv. duration
            0);  // max. num. adv. events
    printf("sl_bt_advertiser_set_timing %d\r\n", sc);
    // client mode. We only need to show the UUID of client, RND ID and wad type
    bleDoomClientAdv_t adv;
    bleDoomClientScan_t scanRsp;

    // set up rndId so that the host can find back us
    adv.lenFlags = 2;
    adv.typeFlags = 1;
    adv.flags = 0x06;
    //
    adv.lenMfgSpecific = 3 + sizeof (adv.rndId);
    adv.typeMfgSpecific = 0xFF;
    adv.mfgId = 0x2FFF; // Silicon Labs
    adv.rndId = settings->rndId;
    // set up client UUID so host can find that we are playing as doom client.
    adv.len128uuid = 0x11;
    adv.type128uuid = 0x07;
    memcpy(&adv.uuid128, static_gattdb->attributes[gattdb_doom_client - 1].constdata->data, sizeof(adv.uuid128));
    sc = sl_bt_legacy_advertiser_set_data(advertising_set_handle, sl_bt_advertiser_advertising_data_packet,
                                                 sizeof (bleDoomHostAdv_t),
                                                 (uint8_t*) &adv);
    app_assert_status(sc);
    // scan response packet
    scanRsp.typeShortName = 0x08;
    const char sn[sizeof(scanRsp.shortName)] = {'D','O','O','M','C','L','N','T'};
    scanRsp.lenShortName = 1 + sizeof(scanRsp.shortName);
    memcpy(&scanRsp.shortName, sn, sizeof(scanRsp.shortName));
    // copy client name
    char cn[sizeof(scanRsp.clientName)];
    memcpy(cn, localPlayerName, sizeof(cn));
    scanRsp.lenMfgSpecific = 3 + sizeof (scanRsp.clientName);
    scanRsp.typeMfgSpecific = 0xFF;
    scanRsp.mfgId = 0x2FFF; // Silicon Labs

    memcpy(&scanRsp.clientName, cn, sizeof(scanRsp.clientName));



    sc = sl_bt_legacy_advertiser_set_data(advertising_set_handle, sl_bt_advertiser_scan_response_packet,
                                                 sizeof (scanRsp),
                                                 (uint8_t*) &scanRsp);


    app_assert_status(sc);
    sc = sl_bt_legacy_advertiser_start(advertising_set_handle, sl_bt_advertiser_connectable_scannable);
    delay(100);      // FIXME: there seems to be a race
    app_assert_status(sc);

  }
  //
}
void bleFreeHostData()
{
  if (NULL != pHostData)    // free only if freed
  {
    Z_Free((void*)pHostData);
  }
  pHostData = NULL;
}
void bleAllocateHostData()
{
  if (NULL == pHostData)      // only allocate if not previously done
  {
    pHostData = Z_Calloc(NET_MAX_HOST_LIST, sizeof(*pHostData), PU_STATIC, NULL);
  }
  else
  {
      memset(pHostData, 0, sizeof (*pHostData));
  }
}
#endif
