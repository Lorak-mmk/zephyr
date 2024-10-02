PROJECT_PATH ?= ${PWD}

TARGET_BINARY ?= /home/lorak/studia/magisterka/tock/target/thumbv7m-none-eabi/release/ti-cc2650-smartrf06-ieee802154_tx.elf
BINFILE_NAME = build/zephyr/zephyr.bin
HEXFILE_NAME = build/zephyr/zephyr.hex

OBJDUMP_FLAGS += --disassemble --disassembler-options=force-thumb,reg-names-std
OBJDUMP_FLAGS += --visualize-jumps=extended-color -w -C

DOCKER_HENI_IMAGE = ghcr.io/mimuw-distributed-systems-group/heni_client:heni
DOCKER_MOUNTS += -v ${HOME}/.mim-dsg/heni:/home/heni/.heni -v ${PROJECT_PATH}:${PROJECT_PATH}
DOCKER_MOUNTS += --mount type=bind,source=/dev,target=/dev

HENI_CMD = docker run --privileged --rm -it --user=1000 -w=${PROJECT_PATH} ${DOCKER_MOUNTS} ${DOCKER_HENI_IMAGE} heni

GDB_PATH=/home/lorak/.local/zephyr-sdk-0.16.3/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb-py
GDB_COMMANDS += --eval-command "tar remote 127.0.0.1:3333" --eval-command "set print asm-demangle on" --eval-command "set history save on" --eval-command "add-symbol-file /home/lorak/studia/magisterka/libtock-rs/target/cc2650dk/thumbv7m-none-eabi/release/examples/ieee802154_tx.tbf" #--eval-command "layout split" --eval-command "focus cmd"
# GDB_COMMANDS += --eval-command "layout split"

BOARD_DK = cc2650_devboard
BOARD_CM = CherryMote
CHERRY_DEV ?= 35d
SAMPLE ?= samples/basic/blinky

.PHONY: clean build-dk build-cm openocd flask-dk flash-cm readelf armdump nm debug

build-dk:
	west build -b ${BOARD_DK} ${SAMPLE} --pristine

build-cm:
	west build -b ${BOARD_CM} ${SAMPLE} --pristine

openocd:
	openocd -f flash-kernel.openocd

flash-dk:
	${HENI_CMD} n prog dk ${HEXFILE_NAME}

flash-cm:
	${HENI_CMD} n prog cherry -d dev:${CHERRY_DEV} ${HEXFILE_NAME}

readelf:
	readelf -WaC ${TARGET_BINARY}

armdump:
	arm-none-eabi-objdump -m arm ${OBJDUMP_FLAGS} ${TARGET_BINARY}

nm:
	nm -f just-symbols ${TARGET_BINARY}

debug:
	${GDB_PATH} ${TARGET_BINARY} ${GDB_COMMANDS}

clean:
	cargo clean

cm-full:
	${HENI_CMD} n uart full ${CHERRY_DEV}

cm-lite:
	${HENI_CMD} n uart lite ${CHERRY_DEV}

dev-full:
	picocom -b 115200 -r -l "/dev/$$(basename $$(dirname $$(grep -lr '01' $$(dirname $$(grep -lr 'XDS100v3' /sys/bus/usb/devices/*/product))/*/bInterfaceNumber))/ttyUSB*)" --imap lfcrlf --omap crlf  -f h

dev-lite:
	picocom -b 230400 -r -l "/dev/$$(basename $$(dirname $$(grep -lr 'CP2102 USB to UART' /sys/bus/usb/devices/*/product))/*:1.0/ttyUSB*)"
