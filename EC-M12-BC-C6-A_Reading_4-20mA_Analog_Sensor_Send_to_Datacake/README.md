# EC-M12-BC-C6-A 4-20mA Sensor: Default vs Battery Powered

This folder contains a battery-optimized variant of the original sketch.

## Files

- **Default version**  
  `EC-M12-BC-C6-A_Reading_4-20mA_Analog_Sensor_Send_to_Datacake/EC-M12-BC-C6-A_Reading_4-20mA_Analog_Sensor_Send_to_Datacake.ino`
- **Battery-powered version**  
  `EC-M12-BC-C6-A_Reading_4-20mA_Analog_Sensor_Send_to_Datacake_Battery_powered/EC-M12-BC-C6-A_Reading_4-20mA_Analog_Sensor_Send_to_Datacake_Battery_powered.ino`

## Main Difference

| Item | Default | Battery-powered |
|---|---|---|
| Debug serial startup (`Serial.begin`) | Enabled | Removed |
| `Serial` / `SerialMon` debug prints | Real UART output | Redirected to a no-output stub |
| Sensor / modem logic | Same | Same |
| RTC logic | Same | Same |
| ADS1115 logic | Same | Same |

## Why a Battery-Powered Version?

On battery operation, continuous debug UART output is usually unnecessary and can increase current usage.

The battery version disables debug printing without changing modem communication (`Serial2`) or RS485 communication (`Serial1`).

## How Users Can Identify the Issue

Use this checklist:

1. If your firmware runs but you do not need serial logs, use the **battery-powered** version.
2. If you need startup/debug logs for troubleshooting modem/network/RTC/I2C issues, use the **default** version.
3. If code fails because of missing/unused debug serial configuration in your target setup, use the **battery-powered** version (debug output removed).

## Important Note

The battery-powered file only removes debug output paths. Functional behavior for sensing, MQTT publish, RTC handling, and power-cycle flow is intentionally unchanged.
