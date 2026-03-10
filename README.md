# FOC Telemetry Parser

A C++20 serial telemetry reader built with Boost.Asio, CMake, and vcpkg.  

It reads framed telemetry from the [FOC firmware](https://gitlab.cern.ch/smm-rme/radiation-tolerant-systems/motor-driver-systems/field-oriented-control-samd21) over a USART link, validates each frame using CRC-16-CCITT, and decodes the 44-byte telemetry payload (at least for now) into meaningful information.

Goals:
- Move telemetry parser, decoder and watchdog to C++
- Keep only PSU control in Python (logging, power cycling etc.)
- The intercommunication is a WIP

---

# Telemetry Frame Format

Telemetry frames follow this format:

```

sync0  sync1  len  type  payload...  crcLo crcHi
0xA5   0x5A   LEN  0x01  PAYLOAD     CRC16(type + payload)

```

Where:

- `LEN = 1 + payload_size`
- `type = 0x01` identifies telemetry messages
- CRC is CRC-16-CCITT (0x1021) computed over `type + payload`

---

# 44-Byte Payload Layout

The current decoder supports the 44-byte payload format:

```
<II6iIiI

```

Which maps to the following fields:

| Field | Type | Description |
|------|------|-------------|
| seq | uint32 | Frame sequence counter |
| t_us | uint32 | Timestamp (microseconds) |
| ia_mA | int32 | Phase A current |
| ib_mA | int32 | Phase B current |
| id_mA | int32 | d-axis current |
| iq_mA | int32 | q-axis current |
| id_ref_mA | int32 | d-axis reference |
| iq_ref_mA | int32 | q-axis reference |
| angle_raw | uint32 | Electrical angle |
| omega_mrad_s | int32 | Angular velocity |
| encoder_error_code | uint32 | Encoder error flags |

---

# Building from Source

To build the project from source, the following are required:

- **CMake ≥ 3.21**
- **MSVC / Visual Studio C++ toolchain**
- **Git**
- **vcpkg**
- **Boost.Asio** (installed through vcpkg)

Optional:

- **CLion** IDE

---

# Installing Dependencies

Clone the repository and its submodules:

1. ```powershell
    git clone https://gitlab.cern.ch/smm-rme/radiation-tolerant-systems/motor-driver-systems/foc-telemetry-parser.git
    ```

2. ```powershell
    cd foc-telemetry-parser
    ```

3. ```powershell
    git submodule update --init --recursive
    ```

Bootstrap **vcpkg**:

```powershell
.\external\vcpkg\bootstrap-vcpkg.bat
```

Dependencies should be installed automatically by vcpkg manifest mode when CMake configures the project

---

# Building with CLion

1. Open the project folder in **CLion**

2. Configure the toolchain

Use:

* **MSVC (Visual Studio toolchain)**
* **CMake**
* **Ninja** or **NMake**

3. Ensure the CMake toolchain file is set:

```
external/vcpkg/scripts/buildsystems/vcpkg.cmake
```

Enable the `debug` preset. Example `CMakePresets.json`:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/debug",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/external/vcpkg/scripts/buildsystems/vcpkg.cmake"
      }
    }
  ]
}
```

4. Reload CMake

CLion will automatically install dependencies via vcpkg

5. Build the project

CLion will produce an executable such as:

```
build/debug/serial_app.exe
```

6. Run the program

To be determined

---

# Building from PowerShell (No IDE)

Open PowerShell in the project root

### 1. Bootstrap vcpkg

```powershell
.\external\vcpkg\bootstrap-vcpkg.bat
```

### 2. Configure CMake

```powershell
cmake -S . -B build `
 -G Ninja `
 -DCMAKE_TOOLCHAIN_FILE="$PWD\external\vcpkg\scripts\buildsystems\vcpkg.cmake"
```

### 3. Build

```powershell
cmake --build build
```

---

# Serial Port Notes (Windows)

For ports `COM1` – `COM9`:

```cpp
"COM3"
```

For `COM10` and higher:

```cpp
R"(\\.\COM10)"
```

---

# Planned Improvements

Future work may include:

* support for additional payload formats (88, 92, 116 bytes)
* CSV telemetry logging
* reconnect logic
* optional asynchronous serial reader
* integration with Python test automation scripts
