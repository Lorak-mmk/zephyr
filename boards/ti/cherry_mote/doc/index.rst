.. _cherry_mote:

CherryMote (1KT)
################

Overview
********

CherryMote is a low-power wireless sensor node designed by researchers at the
University of Warsaw, and is the hardware building block of the 1KT testbed — a
1000-node low-power wireless networking research testbed.

Each CherryMote consists of two components on a custom PCB:

* A **Node** — the Texas Instruments CC2650EM-7ID evaluation module, containing
  the CC2650 MCU, a PCB antenna, and two crystal oscillators.
* A **Supervisor** — a Linux-capable board that manages the Node, provides it
  with power, programs it, and forwards its serial output
  (UART-Lite logs) to the 1KT infrastructure.

The Supervisor is connected to the 1KT main server through a VPN and is
controlled using the ``heni`` tool.

Hardware
********

The Node is equipped with the TI CC2650 MCU, a SimpleLink |trade| wireless MCU
featuring a 48 MHz ARM |reg| Cortex |reg|-M3 core, 128 KB of flash and 20 KB of
SRAM.

The CC2650 contains three separate processing units:

* The **main CPU** (ARM Cortex-M3) running application code.
* The **RF Core** (ARM Cortex-M0) controlling all radio operations
  independently of the CPU.
* The **Sensor Controller** — a 16-bit ultra-low-power proprietary RISC
  processor that can run independently of the main CPU, even while the CPU is
  in a sleep state.

The Node is also equipped with a **TI TMP431** temperature sensor connected to
the main CPU via I2C.

See the `TI CC2650 Product Page`_ for details on the SoC.

Supported Features
==================

The ``cherry_mote`` board configuration supports the following hardware
features:

+-----------------+------------------+---------------------------------------+
| Interface       | Controller       | Driver/Component                      |
+=================+==================+=======================================+
| GPIO            | on-chip          | gpio                                  |
+-----------------+------------------+---------------------------------------+
| NVIC            | on-chip          | arch/arm                              |
+-----------------+------------------+---------------------------------------+
| PINCTRL         | on-chip          | pinctrl                               |
+-----------------+------------------+---------------------------------------+
| UART (Full)     | on-chip          | serial (uart_cc13xx_cc26xx)           |
+-----------------+------------------+---------------------------------------+
| UART-Lite       | Sensor Controller| uart_lite_cc13xx_cc26xx               |
+-----------------+------------------+---------------------------------------+
| I2C             | on-chip          | i2c (i2c_cc13xx_cc26xx)               |
+-----------------+------------------+---------------------------------------+
| IEEE 802.15.4   | on-chip RF Core  | ieee802154_cc13xx_cc26xx              |
+-----------------+------------------+---------------------------------------+
| TMP431          | I2C              | sensor (ti_tmp431)                    |
+-----------------+------------------+---------------------------------------+
| Power Mgmt      | on-chip          | PowerCC26XX                           |
+-----------------+------------------+---------------------------------------+
| TRNG            | on-chip          | entropy                               |
+-----------------+------------------+---------------------------------------+

Connections and IOs
===================

+---------+-------------------+----------------------------------------------+
| DIO Pin | Function          | Usage                                        |
+=========+===================+==============================================+
| DIO2    | UART0_RX          | UART-Full RX (to Supervisor)                 |
+---------+-------------------+----------------------------------------------+
| DIO3    | UART0_TX          | UART-Full TX (to Supervisor)                 |
+---------+-------------------+----------------------------------------------+
| DIO4    | UART0_CTS         | UART-Full CTS (hardware flow control)        |
+---------+-------------------+----------------------------------------------+
| DIO6    | GPIO              | TMP431 power supply (GPIO-controlled)        |
+---------+-------------------+----------------------------------------------+
| DIO8    | UART0_RTS         | UART-Full RTS (hardware flow control)        |
+---------+-------------------+----------------------------------------------+
| DIO11   | GPIO              | Bootloader backdoor pin                      |
+---------+-------------------+----------------------------------------------+
| DIO18   | I2C_MSSDA         | I2C SDA (TMP431 thermometer)                 |
+---------+-------------------+----------------------------------------------+
| DIO19   | I2C_MSSCL         | I2C SCL (TMP431 thermometer)                 |
+---------+-------------------+----------------------------------------------+
| DIO20   | GPIO              | LED (Red)                                    |
+---------+-------------------+----------------------------------------------+

Programming and Debugging
*************************

Programming and debugging are
done remotely through the **Supervisor** device using the ``heni`` tool.
Optionally you can use Zephyr's ``west`` command which has support for using
``heni`` (See ``scripts/west_commands/runners/heni.py`` for implementation
of this integration).

Prerequisites
=============

#. Obtain access to a 1KT CherryMote and install the ``heni`` tool.
   The tool is available as a Docker image at
   ``ghcr.io/mimuw-distributed-systems-group/heni_client:heni`` or can be
   installed natively.

#. Ensure ``west`` is installed as part of the Zephyr development environment.
   See :ref:`getting_started` for details.

#. For debugging, install OpenOCD. It can be obtained via the
   :ref:`Zephyr SDK <toolchain_zephyr_sdk>`.

Flashing
========

CherryMote uses its serial ROM bootloader and the ``heni`` runner. The
``west flash`` command copies the compiled binary to the Supervisor, which
then programs the Node via the serial bootloader.

.. note::

   The ``heni`` runner expects a device ID passed with ``--dev-id``.
   This is the identifier of your CherryMote in the 1KT system.

.. code-block:: console

   west build -b cherry_mote <app_dir>
   west flash --dev-id <device_id>

For example, to build and flash the ``thermometer`` sample application from
the ``apps/`` directory:

.. code-block:: console

   west build -b cherry_mote apps/thermometer
   west flash --dev-id <device_id>

Internally, the runner renames the ``.bin`` output file to ``.flash``
(as required by ``heni``) before invoking:

.. code-block:: console

   heni node prog cherry -d dev:<device_id> zephyr.bin.flash

Debugging
=========

Debugging requires OpenOCD running on the Supervisor. The ``west debug``
command automates the following steps:

#. Upload the OpenOCD configuration file for CherryMote to the Supervisor.
#. Open an OpenOCD server on the Supervisor via SSH.
#. Connect a local GDB session to the remote OpenOCD server.

.. code-block:: console

   west debug --dev-id <device_id>

Alternatively, to start only the OpenOCD server on the Supervisor side:

.. code-block:: console

   west debugserver --dev-id <device_id>

Sample Applications
*******************

Various example applications were developed when working on supporting Zephyr
on CherryMote. They are provided in the ``apps/`` directory on this fork.
They are all built targeting the ``cherry_mote`` board:

``apps/thermometer``
   Reads the ambient temperature from the TMP431 sensor once per second and
   prints the result over UART-Lite. Demonstrates I2C, the TMP431 driver, and
   the sensor API.

``apps/uart_lite``
   Sends numbered messages over UART-Lite continuously. Minimal example of
   the UART-Lite async API.

``apps/1KT_demo``
   Demonstrates IEEE 802.15.4 packet transmission combined with a Zephyr
   shell accessible over UART-Full.

``apps/ieee802_15_4_raw``
   Minimal IEEE 802.15.4 raw-mode send/receive example.

``apps/udp_client`` / ``apps/udp_server``
   UDP networking examples using the IEEE 802.15.4 radio.

To build any of the above:

.. code-block:: console

   west build -b cherry_mote apps/<app_name>
   west flash --dev-id <device_id>

Serial Interfaces
=================

CherryMote exposes two serial interfaces:

* **UART-Full** (``uart0``) — a hardware UART with hardware flow control
  (RTS/CTS). On 1KT, it is used for interactive communication with the Node
  (e.g., a Zephyr shell). The Supervisor exposes this interface over a remote
  terminal connection. It is configured as ``zephyr,shell-uart`` in the
  DeviceTree.

* **UART-Lite** — a UART emulated by the Sensor Controller, operating at
  230400 baud. Because the Sensor Controller runs independently of the main
  CPU, the CPU's only job is to copy a log message into the Sensor Controller's
  RAM; the transmission itself does not block or wake the CPU. This makes
  UART-Lite the preferred output for logging. On 1KT, the Supervisor
  aggregates UART-Lite output and forwards it to the server for later analysis.
  It is configured as ``zephyr,console`` in the DeviceTree.

  .. note::

     UART-Lite is implemented as a child node of the Sensor Controller
     (``sc0``) in the DeviceTree. It uses the
     ``CONFIG_UART_ASYNC_API`` and is accessible via the ``uart-lite``
     alias.

UART-Lite (Sensor Controller)
******************************

UART-Lite is a UART emulated by the CC2650 Sensor Controller. The
implementation consists of two DeviceTree devices and their corresponding
drivers:

**Sensor Controller** (``ti,sensor-controller``)
   The Sensor Controller device manages the Sensor Controller hardware
   interface and firmware. The firmware (provided as a binary blob) is
   the UART emulator code developed for 1KT. The driver is derived from
   the SCIF (Sensor Controller Interface Framework) code, adapted from the
   Contiki-NG 1KT fork (which in turn was adapted from whip6).

**UART-Lite** (``ti,cc13xx-cc26xx-uart-lite``)
   A virtual serial device that is a child of the Sensor Controller node.
   It implements the output portions of Zephyr's async UART API
   (``uart_tx``). Data is written to the Sensor Controller's RAM buffer
   (1536 bytes); if the buffer is full, excess bytes are dropped and a
   truncation notice is prepended to the next message.

To use UART-Lite for console output (the default on CherryMote), ensure
the following options are set in your ``prj.conf``:

.. code-block:: kconfig

   CONFIG_SERIAL=y
   CONFIG_UART_ASYNC_API=y
   CONFIG_LOG_BACKEND_UART_ASYNC=y
   CONFIG_CONSOLE=y
   CONFIG_UART_CONSOLE=y

``CONFIG_LOG_MODE_IMMEDIATE=y`` is also strongly advised, because this board
doesn't really have enough memory for a typical Zephyr log impl with a separate
buffer.

IEEE 802.15.4 Radio
*******************

The CC2650 RF Core provides an IEEE 802.15.4-compliant 2.4 GHz radio.
The driver is shared with the CC2652 family and is enabled by the
``ieee802154`` child node under ``&radio`` in the DeviceTree.

To use raw IEEE 802.15.4 (without a full network stack):

.. code-block:: kconfig

   CONFIG_NETWORKING=y
   CONFIG_IEEE802154=y
   CONFIG_IEEE802154_RAW_MODE=y

   CONFIG_NET_PKT_RX_COUNT=3
   CONFIG_NET_PKT_TX_COUNT=3
   CONFIG_NET_BUF_RX_COUNT=3
   CONFIG_NET_BUF_TX_COUNT=3
   CONFIG_NET_BUF_DATA_SIZE=128

Changing buffer counts and size is required because of extremely small amount
of SRAM memory available on this board.

See the ``apps/1KT_demo`` and ``apps/ieee802_15_4_raw`` applications for
example usage.

TMP431 Temperature Sensor
**************************

The TMP431 is a dual-channel digital temperature sensor connected to the
CC2650 via I2C (address ``0x4c``). It is powered via a GPIO-controlled
supply pin (DIO6) so it can be switched off when not in use.

The sensor is exposed to Zephyr as a standard ``sensor`` device and is
accessible via the ``ambient-temp0`` alias.

To use it, enable the following options:

.. code-block:: kconfig

   CONFIG_SENSOR=y
   CONFIG_I2C=y

Example usage (see ``apps/thermometer`` for a complete application):

.. code-block:: c

   const struct device *dev = DEVICE_DT_GET(DT_ALIAS(ambient_temp0));
   struct sensor_value val;

   sensor_sample_fetch_chan(dev, SENSOR_CHAN_AMBIENT_TEMP);
   sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &val);
   printf("Temperature: %d.%06d °C\n", val.val1, val.val2);

Power Management
****************

System and device power management are supported via
:kconfig:option:`CONFIG_PM` and :kconfig:option:`CONFIG_PM_DEVICE`.

When ``CONFIG_PM=y`` is set, the system may enter standby mode (sleep
state 2). While in standby mode, the UART-Full peripheral is inactive.
If ``uart_poll_in()`` is used in a polling loop, characters may be missed.
To prevent this, hold the standby lock while polling:

.. code-block:: c

   pm_policy_state_lock_get(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
   /* polling loop using uart_poll_in() */
   pm_policy_state_lock_put(PM_STATE_STANDBY, PM_ALL_SUBSTATES);

UART-Lite is unaffected by sleep states because the Sensor Controller
runs in its own power domain independently of the main CPU.

References
**********

.. _TI CC2650 Product Page:
   https://www.ti.com/product/CC2650

.. _TI CC2650 Datasheet:
   https://www.ti.com/lit/pdf/swrs158

.. _TI CC2650 Technical Reference Manual:
   https://www.ti.com/lit/pdf/swcu117

1KT Testbed:
   https://1kt.network
