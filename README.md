DashCAN is an open-source Windows application for monitoring, analyzing, and visualizing CAN bus communication(DashCAN-Overview video link: https://drive.google.com/file/d/1XMqV9JXD6d6TOGpGYja2csdfHu0FnINa/view?usp=drive_link)

**Version:** v1.0.0 (Latest)  
**Status:** Stable Release  

<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/8bea9ddc-c831-4c13-acce-b8b4e7e3b724" />

DashCAN is an open-source Windows application for monitoring, analyzing, and visualizing CAN bus communication.
Developed in C++/Qt (Open Source), DashCAN provides a modern, flexible, and community-driven alternative to proprietary tools such as BusMaster or PCAN-Explorer, with a strong focus on real-time DBC decoding, visualization, and workspace management.

⚙️ Designed for PEAK-System PCAN-USB adapters using the PCAN-Basic API.
📜 Complies fully with Qt LGPLv3 and PEAK-System’s EULA.

🚀 Key Features
Standard DBC file attachment for real-time CAN message parsing and decoding
- 📂 Standard DBC file attachment for real-time CAN message parsing and decoding  
- 🔍 Real-time message monitor — view CAN IDs, names, and decoded signals  
- 📈 Signal plotting — visualize multiple signals over time (value-range vs. time graph)  
- 🎛️  Custom dashboard builder — create your own panels with dials, bars, numeric displays, etc., for live signal monitoring  
- 💾 Data saving — export and save messages, signals, and dashboards from graphs or live data views  
- 🧾 Trace recording — record transmit/receive traffic in **PCAN-Explorer (.trc)** format  
- ⏯️  Trace playback — load and replay recorded trace files for analysis  
- 📡 Transmission control — send one-shot or periodic CAN messages  
- 🔢 Advanced filtering — filter messages by CAN ID or by message name  
- 🔌 PCAN-USB configuration — connect to PEAK hardware, configure standard bitrates  
- 🗂️  Workspace management — organize all project-related files (DBC, logs, traces, configs) in a dedicated workspace  
- 🧹 System logs — view, save, and clear detailed logs  
- 🧾 Device status view — see connected PCAN device and bitrate information in status bar  
- ⚠️  Bus error status monitoring — observe error states and bus conditions in real-time  

🧩 Supported Hardware
DashCAN supports only genuine PEAK-System CAN hardware (e.g., PCAN-USB, PCAN-USB FD).

⚠️ The use of PCANBasic.h, PCANBasic.lib, and PCANBasic.dll is governed by the PEAK-System EULA.[PEAK-System EULA](https://www.peak-system.com/quick/eula).  
These files may only be used with genuine PEAK-System hardware.
 
📖 Learn more at:  
- [PEAK-System Technik GmbH](https://www.peak-system.com)  
- [PCAN-Basic API Documentation](https://www.peak-system.com/PCAN-Basic.239.0.html)

🧰 Dependencies
| Component | Description | License |
|------------|--------------|----------|
| **Qt (Open Source)** | Cross-platform GUI and core framework | [LGPLv3](https://www.gnu.org/licenses/lgpl-3.0.en.html) |
| **PEAK-System PCAN-Basic API** | Windows CAN interface API (for genuine PCAN hardware) | [PEAK EULA](https://www.peak-system.com/quick/eula) |
| **CMake / qmake** | Build tools for project configuration | — |

⚠️ Do **not** include or distribute `PCANBasic.dll`, `.lib`, or `.h` in your repository.  
Users must install PEAK drivers separately.

🏗️ Build Instructions

1️⃣ Prerequisites
- [Qt (Open Source)](https://www.qt.io/download-open-source) (version ≥ 6.5 recommended)  
- [PEAK-System PCAN Drivers](https://www.peak-system.com/quick/DrvSetup)  
- C++17 or later compiler (MSVC or MinGW)

2️⃣ Clone the Repository
```bash
git clone https://github.com/tahir11shaikh/DashCAN.git
cd DashCAN
```

⚖️ Licensing
📜 DashCAN Project License

This project is licensed under the GNU Lesser General Public License v3 (LGPLv3).
You may freely use, modify, and distribute DashCAN under the same license.

A copy of the license is included in the repository under LICENSE and is available online at:
- [GNU Lesser General Public License](https://www.gnu.org/licenses/lgpl-3.0.en.html)  
 

🧱 Qt Framework
DashCAN uses the Qt Open Source Framework under LGPLv3.
Under this license, you must:
Dynamically link Qt libraries (no static linking)
Allow users to relink the application with a modified Qt version
Include the full text of the LGPLv3 license and acknowledge Qt usage
Provide access to the Qt source code used (can be via official Qt site)

Learn more:
🔗 Qt Open Source Licensing Guide
🔌 PEAK-System PCAN-Basic API

The PCAN-Basic API (DLL, LIB, Header) is not open source.
Files are copyrighted by PEAK-System Technik GmbH.
Use and redistribution are allowed only in connection with genuine PEAK hardware.
You may not relicense, modify, or distribute these files.
- [EULA](https://www.peak-system.com/quick/eula) 

🧾 Legal Notices
Qt is a registered trademark of The Qt Company Ltd.
PEAK-System, PCAN, and all related marks are trademarks of PEAK-System Technik GmbH.
DashCAN is an independent open-source project, not affiliated with or endorsed by PEAK-System or The Qt Company.
The developers assume no liability for improper CAN usage or hardware damage.
Use this software responsibly on non-critical networks only.

🤝 Contributing
Contributions are welcome!
Please ensure that:
All contributions remain compliant with LGPLv3
No proprietary PEAK-System binaries or headers are committed
Code is well-documented and tested

🧭 Future Roadmap
- Multi-device simultaneous monitoring
- DBC editor and signal visualization improvements
- SocketCAN (Linux) and serial CAN interface support
- MQTT/BLE remote dashboard monitoring
- Signal trend analysis and export to CSV/JSON
- Advanced trigger and filtering system

🙏 Acknowledgements
Qt Project
 — for providing the open-source GUI framework
PEAK-System Technik GmbH
 — for providing PCAN-Basic API and hardware
The open-source CAN developer community — for inspiration, testing, and feedback

📚 References
- [Qt Licensing](https://www.qt.io/licensing/open-source-lgpl-obligations)
- [LGPLv3 License](https://www.gnu.org/licenses/lgpl-3.0.en.html)
- [GPL FAQ](https://www.gnu.org/licenses/gpl-faq.html)
- [PEAK-System EULA](https://www.peak-system.com/quick/eula)
- [PCAN-Basic API Docs](https://docs.peak-system.com/API/PCAN-Basic/)
