.. zephyr:code-sample:: hello_world
   :name: Hello World (Alif DK-E8)

   Print "Hello World" to the console and blink ``led0`` on DK-E8.

Overview
********

This README documents a working Windows flow to build and flash
``samples/hello_world`` on Alif DK-E8.

Board target used in this guide:

- ``alif_e8_dk/ae822fa0e5597xx0/rtss_he``

Known-Good Paths
****************

- SDK root: ``C:\sdk-alif``
- Zephyr tree: ``C:\sdk-alif\zephyr``
- West: ``C:\sdk-alif\.venv\Scripts\west.exe``
- Security Toolkit: ``C:\alif-se-tools-1.109\app-release-exec``

Activate Environment
********************

PowerShell:

.. code-block:: powershell

   & C:\sdk-alif\.venv\Scripts\Activate.ps1
   cd C:\sdk-alif\zephyr

Optional explicit settings:

.. code-block:: powershell

   $env:ZEPHYR_TOOLCHAIN_VARIANT = "zephyr"
   $env:ALIF_SE_TOOLS_DIR = "C:\alif-se-tools-1.109\app-release-exec"

Build (Build Folder Stays in ``hello_world``)
*********************************************

.. code-block:: bat

   C:\sdk-alif\.venv\Scripts\west.exe build -p always ^
     -b alif_e8_dk/ae822fa0e5597xx0/rtss_he ^
     -d C:\sdk-alif\zephyr\samples\hello_world\build ^
     samples\hello_world

Build artifacts are generated under:

- ``C:\sdk-alif\zephyr\samples\hello_world\build\zephyr\``

Flash
*****

.. code-block:: bat

   C:\sdk-alif\.venv\Scripts\west.exe flash -d C:\sdk-alif\zephyr\samples\hello_world\build

Monitor UART Logs
*****************

1. Open Device Manager and find ``JLink CDC UART Port (COMx)``.
2. Open that COM port in a serial terminal: ``115200``, ``8-N-1``, no flow control.
3. Press RESET on DK-E8.
4. Expected log output includes:
   - ``Hello World! ...``
   - ``Tick 0``, ``Tick 1``, ...
5. ``led0`` also toggles on this sample.

Troubleshooting
***************

COM Port Access Denied
======================

Symptom:

- ``openSerial could not open port 'COMxx': PermissionError(13, 'Access is denied.', ...)``

Fix:

.. code-block:: bat

   taskkill /IM JLinkRTTViewer.exe /F
   taskkill /IM JLinkGUIServer.exe /F

Then retry flash:

.. code-block:: bat

   C:\sdk-alif\.venv\Scripts\west.exe flash -d C:\sdk-alif\zephyr\samples\hello_world\build

Target Did Not Respond (COM Opens, but Flash Fails)
===================================================

Symptom during flash:

- ``[INFO] COMxx open Serial port success``
- ``[ERROR] Target did not respond``

Recovery:

1. Run maintenance:

.. code-block:: bat

   cd /d %ALIF_SE_TOOLS_DIR%
   maintenance.exe -c COM31

2. In the menu select:
   - ``1`` (Device Control)
   - ``1`` (Hard maintenance mode)
3. When ``Waiting for Target..[RESET Platform]`` appears, press RESET on DK-E8.
4. Exit maintenance and retry flash:

.. code-block:: bat

   C:\sdk-alif\.venv\Scripts\west.exe flash -d C:\sdk-alif\zephyr\samples\hello_world\build
