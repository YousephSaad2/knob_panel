# CIS-450 Project: Lighting Control Panel

## Overview
This project implements a lighting control system with synchronized audio feedback. The user can control brightness levels using a knob, with voice announcements reflecting the current brightness.

## Features
- Adjust brightness in 25% increments using a physical knob.
- Real-time voice announcements for brightness levels (e.g., "Brightness 50%").
- Responsive LED interface to reflect changes.
- Well-documented architecture with FreeRTOS tasks and event groups for concurrency control.

## Project Structure
- **`src/`**: Contains all source code and main project files.
- **`docs/`**: Documentation, including system architecture, concurrency explanation, and user guide.
- **`video/`**: Demonstration video showcasing the functionality.
