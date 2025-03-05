👋 Hi, I’m @Abhinn-F5, and this is my work.

>1\. CHAT MANAGEMENT AND MODERATION SYSTEM
***

## CHAT MANAGEMENT AND MODERATION SYSTEM

Overview

This project is a POSIX-compliant C implementation of a real-time chat management and moderation system, designed as an assignment for the Operating Systems (CS-F372) course. The system simulates a chat platform where users, groups, and a moderator communicate via Inter-Process Communication (IPC) mechanisms like message queues and pipes. The system monitors conversations and enforces moderation by filtering messages that contain restricted words.

Requires **Ubuntu 22.04 or 24.04** and a C compiler and POSIX libraries.

**Features**

- **Multi-process architecture**: Each chat group and user operates as separate processes.
- **Message filtering & moderation**: Messages are checked for restricted words, and users exceeding violation thresholds are removed.
- **Ordered message delivery**: Ensures that messages are processed in correct timestamp order.
- **Real-time banning**: Users are removed if they exceed the violation threshold.
- **Automated validation**: Messages are checked by an external validation script (validation.out).

**System Architecture**

- **app.c**: Main process that initializes and manages group processes.
- **groups.c**: Handles group processes, user messages, and communication with the moderator and validation system.
- **moderator.c**: Monitors messages, flags violations, and removes users if necessary.
- **validation.out**: An external binary that verifies message ordering and correctness.

**IPC Mechanisms Used**

- **Message Queues**: Used for communication between groups, moderator, and validation.
- **Unnamed Pipes**: Used for interaction between user and group processes.

**Execution Flow**

1. Start validation.out (test case execution).
2. Start moderator.out to handle moderation.
3. Run app.out, which initializes and manages group processes.

<!---
Abhinn-F5/Abhinn-F5 is a ✨ special ✨ repository because its `README.md` (this file) appears on your GitHub profile.
You can click the Preview link to take a look at your changes.
--->
