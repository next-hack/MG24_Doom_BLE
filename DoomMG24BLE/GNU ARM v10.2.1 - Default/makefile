################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/ff/subdir.mk
-include src/subdir.mk
-include gecko_sdk_4.3.2/util/third_party/mbedtls/library/subdir.mk
-include gecko_sdk_4.3.2/util/silicon_labs/silabs_core/memory_manager/subdir.mk
-include gecko_sdk_4.3.2/protocol/bluetooth/src/subdir.mk
-include gecko_sdk_4.3.2/platform/service/system/src/subdir.mk
-include gecko_sdk_4.3.2/platform/service/sleeptimer/src/subdir.mk
-include gecko_sdk_4.3.2/platform/service/power_manager/src/subdir.mk
-include gecko_sdk_4.3.2/platform/service/hfxo_manager/src/subdir.mk
-include gecko_sdk_4.3.2/platform/service/device_init/src/subdir.mk
-include gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/subdir.mk
-include gecko_sdk_4.3.2/platform/security/sl_component/sl_protocol_crypto/src/subdir.mk
-include gecko_sdk_4.3.2/platform/security/sl_component/sl_mbedtls_support/src/subdir.mk
-include gecko_sdk_4.3.2/platform/security/sl_component/se_manager/src/subdir.mk
-include gecko_sdk_4.3.2/platform/radio/rail_lib/plugin/rail_util_sequencer/subdir.mk
-include gecko_sdk_4.3.2/platform/radio/rail_lib/plugin/rail_util_power_manager_init/subdir.mk
-include gecko_sdk_4.3.2/platform/radio/rail_lib/plugin/pa-conversions/subdir.mk
-include gecko_sdk_4.3.2/platform/radio/rail_lib/plugin/fem_util/subdir.mk
-include gecko_sdk_4.3.2/platform/peripheral/src/subdir.mk
-include gecko_sdk_4.3.2/platform/emlib/src/subdir.mk
-include gecko_sdk_4.3.2/platform/emdrv/nvm3/src/subdir.mk
-include gecko_sdk_4.3.2/platform/common/toolchain/src/subdir.mk
-include gecko_sdk_4.3.2/platform/common/src/subdir.mk
-include gecko_sdk_4.3.2/platform/Device/SiliconLabs/MGM24/Source/subdir.mk
-include autogen/subdir.mk
-include Doom/source/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := DoomMG24BLE
BUILD_ARTIFACT_EXTENSION := axf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: main-build

# Main-build Target
main-build: DoomMG24BLE.axf

# Tool invocations
DoomMG24BLE.axf: $(OBJS) $(USER_OBJS) makefile objects.mk $(OPTIONAL_TOOL_DEPS)
	@echo 'Building target: $@'
	@echo 'Invoking: GNU ARM C Linker'
	arm-none-eabi-gcc -g -gdwarf-2 -mcpu=cortex-m33 -mthumb -T "C:\Users\niwrachi\SimplicityStudio\v5_workspace\GitHub\MG24Doom\DoomMG24BLE\linkerfile.ld" -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=free -Xlinker --gc-sections -Xlinker -Map="DoomMG24BLE.map" -mfpu=fpv5-sp-d16 -mfloat-abi=hard --specs=nano.specs -o DoomMG24BLE.axf -Wl,--start-group "./Doom/source/am_map.o" "./Doom/source/d_client.o" "./Doom/source/d_items.o" "./Doom/source/d_main.o" "./Doom/source/doom_iwad.o" "./Doom/source/f_finale.o" "./Doom/source/f_wipe.o" "./Doom/source/g_game.o" "./Doom/source/global_data.o" "./Doom/source/hu_lib.o" "./Doom/source/hu_stuff.o" "./Doom/source/i_audio.o" "./Doom/source/i_main.o" "./Doom/source/i_video.o" "./Doom/source/info.o" "./Doom/source/m_bbox.o" "./Doom/source/m_cheat.o" "./Doom/source/m_menu.o" "./Doom/source/m_random.o" "./Doom/source/mus.o" "./Doom/source/opl2.o" "./Doom/source/p_ceilng.o" "./Doom/source/p_doors.o" "./Doom/source/p_enemy.o" "./Doom/source/p_floor.o" "./Doom/source/p_genlin.o" "./Doom/source/p_inter.o" "./Doom/source/p_lights.o" "./Doom/source/p_map.o" "./Doom/source/p_maputl.o" "./Doom/source/p_mobj.o" "./Doom/source/p_plats.o" "./Doom/source/p_pspr.o" "./Doom/source/p_setup.o" "./Doom/source/p_sight.o" "./Doom/source/p_spec.o" "./Doom/source/p_switch.o" "./Doom/source/p_telept.o" "./Doom/source/p_tick.o" "./Doom/source/p_user.o" "./Doom/source/r_data.o" "./Doom/source/r_draw.o" "./Doom/source/r_fast_stuff.o" "./Doom/source/r_main.o" "./Doom/source/r_patch.o" "./Doom/source/r_plane.o" "./Doom/source/r_things.o" "./Doom/source/s_sound.o" "./Doom/source/sounds.o" "./Doom/source/st_gfx.o" "./Doom/source/st_lib.o" "./Doom/source/st_stuff.o" "./Doom/source/tables.o" "./Doom/source/v_video.o" "./Doom/source/w_wad.o" "./Doom/source/wi_stuff.o" "./Doom/source/z_bmalloc.o" "./Doom/source/z_zone.o" "./autogen/gatt_db.o" "./autogen/sl_bluetooth.o" "./autogen/sl_device_init_clocks.o" "./autogen/sl_event_handler.o" "./autogen/sl_power_manager_handler.o" "./gecko_sdk_4.3.2/platform/Device/SiliconLabs/MGM24/Source/startup_mgm24.o" "./gecko_sdk_4.3.2/platform/Device/SiliconLabs/MGM24/Source/system_mgm24.o" "./gecko_sdk_4.3.2/platform/common/src/sl_assert.o" "./gecko_sdk_4.3.2/platform/common/src/sl_slist.o" "./gecko_sdk_4.3.2/platform/common/toolchain/src/sl_memory.o" "./gecko_sdk_4.3.2/platform/emdrv/nvm3/src/nvm3_default_common_linker.o" "./gecko_sdk_4.3.2/platform/emdrv/nvm3/src/nvm3_hal_flash.o" "./gecko_sdk_4.3.2/platform/emdrv/nvm3/src/nvm3_lock.o" "./gecko_sdk_4.3.2/platform/emlib/src/em_burtc.o" "./gecko_sdk_4.3.2/platform/emlib/src/em_cmu.o" "./gecko_sdk_4.3.2/platform/emlib/src/em_core.o" "./gecko_sdk_4.3.2/platform/emlib/src/em_emu.o" "./gecko_sdk_4.3.2/platform/emlib/src/em_eusart.o" "./gecko_sdk_4.3.2/platform/emlib/src/em_gpio.o" "./gecko_sdk_4.3.2/platform/emlib/src/em_i2c.o" "./gecko_sdk_4.3.2/platform/emlib/src/em_ldma.o" "./gecko_sdk_4.3.2/platform/emlib/src/em_msc.o" "./gecko_sdk_4.3.2/platform/emlib/src/em_prs.o" "./gecko_sdk_4.3.2/platform/emlib/src/em_se.o" "./gecko_sdk_4.3.2/platform/emlib/src/em_system.o" "./gecko_sdk_4.3.2/platform/emlib/src/em_timer.o" "./gecko_sdk_4.3.2/platform/emlib/src/em_usart.o" "./gecko_sdk_4.3.2/platform/emlib/src/em_vdac.o" "./gecko_sdk_4.3.2/platform/peripheral/src/peripheral_sysrtc.o" "./gecko_sdk_4.3.2/platform/radio/rail_lib/plugin/fem_util/sl_fem_util.o" "./gecko_sdk_4.3.2/platform/radio/rail_lib/plugin/pa-conversions/pa_conversions_efr32.o" "./gecko_sdk_4.3.2/platform/radio/rail_lib/plugin/rail_util_power_manager_init/sl_rail_util_power_manager_init.o" "./gecko_sdk_4.3.2/platform/radio/rail_lib/plugin/rail_util_sequencer/sl_rail_util_sequencer.o" "./gecko_sdk_4.3.2/platform/security/sl_component/se_manager/src/sl_se_manager.o" "./gecko_sdk_4.3.2/platform/security/sl_component/se_manager/src/sl_se_manager_attestation.o" "./gecko_sdk_4.3.2/platform/security/sl_component/se_manager/src/sl_se_manager_cipher.o" "./gecko_sdk_4.3.2/platform/security/sl_component/se_manager/src/sl_se_manager_entropy.o" "./gecko_sdk_4.3.2/platform/security/sl_component/se_manager/src/sl_se_manager_hash.o" "./gecko_sdk_4.3.2/platform/security/sl_component/se_manager/src/sl_se_manager_key_derivation.o" "./gecko_sdk_4.3.2/platform/security/sl_component/se_manager/src/sl_se_manager_key_handling.o" "./gecko_sdk_4.3.2/platform/security/sl_component/se_manager/src/sl_se_manager_signature.o" "./gecko_sdk_4.3.2/platform/security/sl_component/se_manager/src/sl_se_manager_util.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_mbedtls_support/src/sl_mbedtls.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_mbedtls_support/src/sl_psa_crypto.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_mbedtls_support/src/sli_psa_crypto.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_protocol_crypto/src/sli_protocol_crypto_radioaes.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_protocol_crypto/src/sli_radioaes_management.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sl_psa_its_nvm3.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_psa_driver_common.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_psa_driver_init.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_psa_trng.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_se_driver_aead.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_se_driver_builtin_keys.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_se_driver_cipher.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_se_driver_key_derivation.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_se_driver_key_management.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_se_driver_mac.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_se_driver_signature.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_se_opaque_driver_aead.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_se_opaque_driver_cipher.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_se_opaque_driver_mac.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_se_opaque_key_derivation.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_se_transparent_driver_aead.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_se_transparent_driver_cipher.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_se_transparent_driver_hash.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_se_transparent_driver_mac.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_se_transparent_key_derivation.o" "./gecko_sdk_4.3.2/platform/security/sl_component/sl_psa_driver/src/sli_se_version_dependencies.o" "./gecko_sdk_4.3.2/platform/service/device_init/src/sl_device_init_dcdc_s2.o" "./gecko_sdk_4.3.2/platform/service/device_init/src/sl_device_init_dpll_s2.o" "./gecko_sdk_4.3.2/platform/service/device_init/src/sl_device_init_emu_s2.o" "./gecko_sdk_4.3.2/platform/service/device_init/src/sl_device_init_hfxo_mgm24.o" "./gecko_sdk_4.3.2/platform/service/device_init/src/sl_device_init_lfrco.o" "./gecko_sdk_4.3.2/platform/service/device_init/src/sl_device_init_nvic.o" "./gecko_sdk_4.3.2/platform/service/hfxo_manager/src/sl_hfxo_manager.o" "./gecko_sdk_4.3.2/platform/service/hfxo_manager/src/sl_hfxo_manager_hal_s2.o" "./gecko_sdk_4.3.2/platform/service/power_manager/src/sl_power_manager.o" "./gecko_sdk_4.3.2/platform/service/power_manager/src/sl_power_manager_debug.o" "./gecko_sdk_4.3.2/platform/service/power_manager/src/sl_power_manager_hal_s2.o" "./gecko_sdk_4.3.2/platform/service/sleeptimer/src/sl_sleeptimer.o" "./gecko_sdk_4.3.2/platform/service/sleeptimer/src/sl_sleeptimer_hal_burtc.o" "./gecko_sdk_4.3.2/platform/service/sleeptimer/src/sl_sleeptimer_hal_sysrtc.o" "./gecko_sdk_4.3.2/platform/service/sleeptimer/src/sl_sleeptimer_hal_timer.o" "./gecko_sdk_4.3.2/platform/service/system/src/sl_system_init.o" "./gecko_sdk_4.3.2/platform/service/system/src/sl_system_process_action.o" "./gecko_sdk_4.3.2/protocol/bluetooth/src/sl_bt_stack_init.o" "./gecko_sdk_4.3.2/protocol/bluetooth/src/sli_bt_accept_list_config.o" "./gecko_sdk_4.3.2/protocol/bluetooth/src/sli_bt_advertiser_config.o" "./gecko_sdk_4.3.2/protocol/bluetooth/src/sli_bt_connection_config.o" "./gecko_sdk_4.3.2/protocol/bluetooth/src/sli_bt_dynamic_gattdb_config.o" "./gecko_sdk_4.3.2/protocol/bluetooth/src/sli_bt_l2cap_config.o" "./gecko_sdk_4.3.2/protocol/bluetooth/src/sli_bt_pawr_advertiser_config.o" "./gecko_sdk_4.3.2/protocol/bluetooth/src/sli_bt_periodic_adv_config.o" "./gecko_sdk_4.3.2/protocol/bluetooth/src/sli_bt_periodic_advertiser_config.o" "./gecko_sdk_4.3.2/protocol/bluetooth/src/sli_bt_sync_config.o" "./gecko_sdk_4.3.2/util/silicon_labs/silabs_core/memory_manager/sl_malloc.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/cipher.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/cipher_wrap.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/constant_time.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/platform.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/platform_util.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/psa_crypto.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/psa_crypto_aead.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/psa_crypto_cipher.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/psa_crypto_client.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/psa_crypto_driver_wrappers.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/psa_crypto_ecp.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/psa_crypto_hash.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/psa_crypto_mac.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/psa_crypto_rsa.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/psa_crypto_se.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/psa_crypto_slot_management.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/psa_crypto_storage.o" "./gecko_sdk_4.3.2/util/third_party/mbedtls/library/threading.o" "./src/audio.o" "./src/display.o" "./src/doom_ble.o" "./src/dynMemWrapper.o" "./src/font8x8.o" "./src/graphics.o" "./src/interleavedSpiFlash.o" "./src/keyboard.o" "./src/main.o" "./src/printf.o" "./src/sharedUsart.o" "./src/ymodem.o" "./src/ff/ff.o" "./src/ff/mmc_spi.o" "C:\Users\niwrachi\SimplicityStudio\v5_workspace\GitHub\MG24Doom\DoomMG24BLE\gecko_sdk_4.3.2\protocol\bluetooth\lib\EFR32XG24\GCC\libbluetooth.a" "C:\Users\niwrachi\SimplicityStudio\v5_workspace\GitHub\MG24Doom\DoomMG24BLE\gecko_sdk_4.3.2\platform\emdrv\nvm3\lib\libnvm3_CM33_gcc.a" "C:\Users\niwrachi\SimplicityStudio\v5_workspace\GitHub\MG24Doom\DoomMG24BLE\gecko_sdk_4.3.2\platform\radio\rail_lib\autogen\librail_release\librail_config_mgm240pb32vna_gcc.a" "C:\Users\niwrachi\SimplicityStudio\v5_workspace\GitHub\MG24Doom\DoomMG24BLE\gecko_sdk_4.3.2\platform\radio\rail_lib\autogen\librail_release\librail_module_efr32xg24_gcc_release.a" -lgcc -lc -lm -lnosys -Wl,--end-group -Wl,--start-group -lgcc -lc -lnosys -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

	@echo 'Building out file: DoomMG24BLE.out'
	arm-none-eabi-objcopy "DoomMG24BLE.axf" "DoomMG24BLE.out"
	@echo ' '

	@echo 'Building hex file: DoomMG24BLE.hex'
	arm-none-eabi-objcopy -O ihex "DoomMG24BLE.axf" "DoomMG24BLE.hex"
	@echo ' '

	@echo 'Building bin file: DoomMG24BLE.bin'
	arm-none-eabi-objcopy -O binary "DoomMG24BLE.axf" "DoomMG24BLE.bin"
	@echo ' '

	@echo 'Building s37 file: DoomMG24BLE.s37'
	arm-none-eabi-objcopy -O srec "DoomMG24BLE.axf" "DoomMG24BLE.s37"
	@echo ' '

	@echo 'Running size tool'
	arm-none-eabi-size "DoomMG24BLE.axf" -A
	@echo ' '

# Other Targets
clean:
	-@$(foreach FILE, $(EXECUTABLES), $(shell $(RM) $(FILE)) )
	-@$(foreach FILE, $(OBJS), $(shell $(RM) $(FILE)) )
	-@$(foreach FILE, $(C_DEPS), $(shell $(RM) $(FILE)) )
	-$(RM) DoomMG24BLE.axf
	-@echo ' '

.PHONY: all clean dependents main-build

-include ../makefile.targets
