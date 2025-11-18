PORT := $(shell ls -1 /dev/USB0 /dev/ttyACM0  2>/dev/null | head -1)
ARCH := $(shell uname -m)

all: flash monitor

# python3 /home/merlin/.arduino15/packages/esp32/tools/esptool_py/4.6/esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode keep --flash_freq keep --flash_size keep 0x0 /tmp/arduino_build_268639/NeoMatrix-FastLED-IR.ino.bootloader.bin 0x8000 /tmp/arduino_build_268639/NeoMatrix-FastLED-IR.ino.partitions.bin 0xe000 /home/merlin/.arduino15/packages/esp32/hardware/esp32/3.0.7/tools/partitions/boot_app0.bin 0x10000 /tmp/arduino_build_268639/NeoMatrix-FastLED-IR.ino.bin 
# /esptool  --chip esp32s3 --port /dev/ttyACM0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode keep --flash_freq keep --flash_size keep 0x0 *.ino.bootloader.bin 0x8000 *.ino.partitions.bin 0x10000 *.ino.bin
flash: ready killscreen
	./esptool --chip esp32s3 --port $(PORT) --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode keep --flash_freq keep --flash_size keep 0x0 $$(ls *.ino.bootloader.bin) 0x8000 $$(ls *.ino.partitions.bin) 0x10000 $$(ls *.ino.bin)

	sleep 1 # time for port to come back
	ls -l $(PORT)

ffat: ready killscreen
	./esptool --chip esp32s3 --port $(PORT) --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode keep --flash_freq keep --flash_size keep 2166784 $$(ls *.fatfs.bin)


	sleep 1 # time for port to come back
	ls -l $(PORT)

monitor: ready killscreen
	screen -S screen $(PORT) 115200

copy:	
	cp /tmp/arduino_build_*/*.bin .

push: copy
	rsync -av -SH -P --delete --delete-after --force . root@rpi3a2:/root/esp32/

restart: killscreen
	./esptool.py --chip esp32 --port $(PORT) --baud 460800 --before default_reset --after hard_reset chip_id 
	ls -l $(PORT)
	screen -S screen $(PORT) 115200


killscreen:
	pkill -f -9 '[Ss][Cc][Rr][Ee][Ee][Nn] .*$(PORT)' || :

ready:
	#dpkg -l python3-serial | grep -q python3 || apt-get install python3-serial
	@echo "Will sync to port $(PORT) for $(ARCH)"
	@ln -snf esptool.$(ARCH) esptool
