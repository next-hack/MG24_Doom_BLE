/***************************************************************************//**
 * @file doom_ble.h
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
#ifndef SRC_DOOM_BLE_H_
#define SRC_DOOM_BLE_H_
#include "d_ticcmd.h"     // for ticcmd_t
#include "doomstat.h"     // for BACKUPTICS
//
#define ENABLE_BLE_DEBUG                0
#define BLE_MODE_HOST                   0
#define BLE_MODE_CLIENT                 1
#define MAX_CLIENTS                     3   // MAXPLAYERS - 1
//
#define BLE_PLAYER_NOT_CONNECTED        0
#define BLE_PLAYER_IS_REMOTE_HOST       1
#define BLE_PLAYER_IS_LOCAL_HOST        2
//
#define BLE_CLIENT_DISCONNECTED         0
#define BLE_CLIENT_CONNECTED            1
//
#define NET_MAX_HOST_LIST               4
#define MAX_HOST_NAME_LENGTH            8    // also player name
//
typedef struct
{
    uint32_t rndId;          // id to be matched by clients willing to join
    //
    uint16_t time;
    uint16_t maxKills;
    //
    uint8_t gameMode : 2;        // 0 through 3
    uint8_t map : 6;        // there are less than 63 maps
    //
    uint8_t clients : 2;    // how many players are there (excluding host)
    uint8_t episode : 2;    // episode - 1
    uint8_t skill : 3;      // between 0 to 4
    uint8_t deathmatch : 1;
    //
    uint8_t monsters : 2;    //deathmatch? Respawn Items? Monsters on?
    uint8_t itemRespawn : 1;
    uint8_t valid : 1;        // used only in the list
    //
    uint8_t name[MAX_HOST_NAME_LENGTH];  // game name. If shorter than MAX_HOST_NAME_LENGTH then null terminated
    //
} __attribute__ ((packed)) multiplayerGameSettings_t;
typedef struct
{
    uint32_t lastSeen;
    multiplayerGameSettings_t settings;
    uint8_t bleAddr[6];
} hostData_t;
enum
{
  BLE_DOOM_CMD_START_LEVEL,
  BLE_DOOM_CMD_SET_PLAYER_INFO,
};
extern hostData_t * pHostData;
/*
 * The client will ask data from server, and it will also provide new data to the server.
 */
typedef struct
{
    int checkNumber;
    int numberOfReceivedTicsByClient;        // 1 = the client received tics to run gametic 0, and so on.
    int numberOfTiccmds;                   // how many tics we are sending in this packet
    // up to BACKUPTICS starting from game tic
    ticcmd_t ticcmds[BACKUPTICS];
    int checkNumber2;
} bleDoomClientTics_t;
// tics from server
typedef struct
{
    uint32_t checkNumber;
    int numberOfTicsReceivedByAll;         // the server will inform a client that he knows that all clients shall run up to numberOfTicsReceivedFromAllClients
    int numberOfNewTics;                            // number of tics in this packet, starting from numberOfTicsReceivedFromAllClients - 1.
    uint8_t playerNumber[MAX_CLIENTS];              // player number for each ticcmds
    uint8_t connMask;                               // connection mask
    ticcmd_t ticcmds[MAX_CLIENTS][BACKUPTICS];
    uint32_t checkNumber2;
} bleDoomOtherPlayerTics_t;
void bleFreeHostData();
void bleAllocateHostData();
uint8_t bleGetGameMode(void);
uint8_t bleMustStartGame(void);
void bleResetMustStartGame(void);
uint8_t bleGetClientStatus(void);
uint8_t bleReadOtherPlayerTics(bleDoomOtherPlayerTics_t * otherPlayerTics);
uint8_t bleGetPlayerData(uint8_t playerNumber, uint8_t *name, uint8_t *addr, uint8_t *handle);
void bleUpdateAdvertisingData(uint8_t mode, multiplayerGameSettings_t *settings);
uint8_t bleReadClientTics(bleDoomClientTics_t *cmd, int playerNumber);
void   bleRequestClientsStartGame(void);
void bleInit(void);
uint8_t bleUpdateTicsToServer(int numberOfTicks);
uint8_t bleHostSendTicsToClient(int playerNumber, int minTicMadeByAll, int minReceivedByAll);
void bleStartScanningForHost(void);
void bleStopScanAndAdvertising(void);
void bleCloseNetwork();
void bleConnectionClose(uint32_t clientNumber);
void bleStartScanningForClients(uint32_t gameRndId);
//
extern char localPlayerName[MAX_HOST_NAME_LENGTH];
#endif /* SRC_DOOM_BLE_H_ */
