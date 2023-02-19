# A Networking Sim

A Cellular Arch Sim written in C using the Raylib library and SQLite Database

Current status:

1. Multiple UE's and BTS's with info stored in DB
2. Area segmented into Blocks with an MSC Handler
3. Connections can be created between UE using commands in terminal for specific periods of time
4. Connections between different areas go thru MSC only
5. MSC are connected with specific bandwidth
6. Each UE and BTS belong to an ISP and behave differently
7. Each UE holds a limited validity which variably changes based on location and BTS and can be recharged from terminal
8. Each UE can hold multiple connections
9. Metrics are displayed on simulation screen

Run cmd : `./shell.sh`
