flash: killscreen
	# python /home/merlin/Arduino/hardware/espressif/esp32/tools/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0xe000 /home/merlin/Arduino/hardware/espressif/esp32/tools/partitions/boot_app0.bin 0x1000 /home/merlin/Arduino/hardware/espressif/esp32/tools/sdk/bin/bootloader_qio_80m.bin 0x10000 /tmp/arduino_build_17110/NeoMatrix-FastLED-IR.ino.bin 0x8000 /tmp/arduino_build_17110/NeoMatrix-FastLED-IR.ino.partitions.bin 
	./esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x10000 $$(ls *.ino.bin)
	sleep 1 # time for ttyUSB0 to come back
	ls -l /dev/ttyUSB0
	screen -S screen /dev/ttyUSB0 115200

copy:	
	cp /tmp/arduino_build_*/*.bin .

push: copy
	rsync -av -SH -P --delete --delete-after --force . root@rpi3a2:/root/esp32/

restart: killscreen
	./esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 --before default_reset --after hard_reset chip_id 
	ls -l /dev/ttyUSB0
	screen -S screen /dev/ttyUSB0 115200


killscreen:
	pkill -f -9 'scree[n] .*/dev/ttyUSB0' || :
