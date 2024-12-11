# kvm-dbus-monitor

## Description

**kvm-dbus-monitor** is designed to monitor D-Bus signals and provide a D-Bus interface that facilitates configuration and the triggering of various functionalities[screenshot,video Record etc].The main
goal is to enhance the inter-process communication within the system, allowing for a streamlined configuration process and efficiently react to certain operations triggered by monitored specific
events.

## Features
kvm-dbus-monitor is responsible for following:
- **D-Bus Signal Monitoring**: Listens for specific D-Bus signals emitted by other services or certain events to emits D-Bus trigger signal for certain action.
- **Configuration Interface**: Offers D-Bus interface through which users or other services can update configurations dynamically for certain functionality.
- **Triggering Functionalities**: Provides methods that can be called via D-Bus to emit Trigger signal for specific actions.

## Architecture
The service is built using `sdbusplus::asio` to handle asynchronous D-Bus operations, ensuring a responsive and non-blocking design. Key components include:

- **Signal Listener**: Monitors and reacts to relevant D-Bus signals.
- **Configuration Manager**: Manages runtime configuration updates based on received D-Bus messages.
- **Trigger Handler**: Facilitates the invocation of Triggerring signals for specific action,by validating the trigger condition.

## Requirements
- **C++17 or later**
- **sdbusplus library**
- **boost::asio**
- **nlohmann/json**

## Installation
1. Clone the repository:
   ```bash
   git clone https://git.ami.com/core/ami-bmc/one-tree/core/obmc-ikvm

## Configuration

Upon process startup, kvm-dbus-monitor reads its config file, with the following
structure:

```json
    "Kvm" :{                        # Indicates the object Path Kvm
        "Screenshot" : {            # Indicates the Interface Screenshot
            "Trigger": true         # Trigger Property for Screenshot Signal
        },
        "VideoRecord":{             # Indicates the videoRecord Interface
            "RecordStatus" : false  # Indicates the video record status
        }
    },
```
## Usage
### Starting the Service
To `start` the service, use the systemd command:
```bash
systemctl start kvm-dbus-monitor.service
```

### Stopping the Service
To `stop` the service, run:
```bash
systemctl stop kvm-dbus-monitor.service
```
### Checking Service Status
To view the status of the service:
```bash
systemctl status kvm-dbus-monitor.service
```

## DBus Interface

kvm-dbus-monitor will expose the following object structure. All object paths are
representation of configuration file described in Configuration section.

```ascii
/xyz/openbmc_project/Kvm
/xyz/openbmc_project/Kvm/VideoRecord
```

### Interfces exposed by Kvm object

`Kvm` object will expose configuration of its own under following Interfaces.<br>
- interface: `xyz.openbmc_project.Kvm.Screenshot` (all properties are RO),
which will be defined as follow:

| NAME     | TYPE     | SIGNATURE | RESULT/RETURN | DESCRIPTION                                                         |
| -------- | -------- | --------- | ------------- | ------------------------------------------------------------------- |
| TriggerScreenshot      | Method | INT32     | STRING | Perform an asynchronous operation of emitting Screenshot Trigger Signal,with parameters:<br><br>`INT32` : As of now Only value allowed is `1`   |
| Trigger    | Property | -     | BOOLEAN | `True`, currently it only have is kept as true    |

- interface: `xyz.openbmc_project.Kvm.VideoRecord` (all properties are RO),
which will be defined as follow:

| NAME     | TYPE     | SIGNATURE | RESULT/RETURN | DESCRIPTION                                                         |
| -------- | -------- | --------- | ------------- | ------------------------------------------------------------------- |
| TriggerRecord      | Method | STRING    | STRING | Perform an asynchronous operation of emitting Video Record Trigger Signal,with parameters:<br><br>`STRING` : As of now  Allowed values are :`Start` and `Stop`. |
| RecordStatus   | Property | BOOLEAN     | - | `True`, if object is occupied by active process, `False` otherwise  |

### Interfces exposed by VideoRecord object

`VideoRecord` object will expose configuration of its own under the following Interfaces.<br>
- interface: `xyz.openbmc_project.Kvm.VideoRecord.RemoteStorage` (all properties are RO),
which will be defined as follow:

| NAME     | TYPE     | SIGNATURE | RESULT/RETURN | DESCRIPTION                                                         |
| -------- | -------- | --------- | ------------- | ------------------------------------------------------------------- |
| EnableRemoteStorage | Method | BOOLEAN | STRING | provides option for remote storage enable disable,with parameters:<br><br>`BOOLEAN` : to Set the `RecordToRemote` property   |
| UpdateRemoteStorageInfo      | Method | BYTE<br>BYTE<br>BYTE<br>STRING<br>STRING<br>STRING<br>VARIANT<INT32,UNIX_FD,INT> | STRING | Perform an asynchronous operation of mounting the Remote storage to BMC and updating Remote Storage Configuration, with parameters:<br><br>`BYTE`:value to update `MaxDumps` property. <br>`BYTE`:value to update `MaxDuration` property.<br>`BYTE`:value to update `MaxSize` property.<br>`STRING`:value to update `serverIP` property.<br>`STRING`: value to update `PathInServer` property. <br>`STRING` : value to update `ShareType` property. <br>`VARIANT<INT32,UNIX_FD>` : file descriptor of named pipe used for passing null-delimited secret data (username and password). When there is no data to pass `-1` should be passed as `INT32` |
| Active | Property | - | BOOLEAN | `True`, if active Remote Storage is in progress, `False` otherwise  |
| MaxDumps | Property | BYTE | BYTE | currently only value `1` is Allowed. |
| MaxDuration | Property | BYTE | BYTE |currently allowed values: `1` to `20` (in `sec`) |
| MaxSize | Property | BYTE | BYTE | `maxSize` : currently allowed values `1` to `10` (in `MB`) |
| PathInServer | Property | STRING | STRING | `pathInServer`: Requires a valid path in remote storage with prefixed `'/'`.  |
| RecordToRemote | Property | BOOLEAN | BYTE | `True`, if remote storage is enabled ,`False` otherwise |
| ServerIP | Property | STRING | STRING | `serverIP` currently only ipv4 values allowed |
| ShareType | Property | STRING | STRING | `shareType` cuurently allowed values are `nfs` and `cifs`. |

- interface: `xyz.openbmc_project.Kvm.VideoRecord.TriggerSettings` (all properties are RO),
which will be defined as follow:

| NAME     | TYPE     | SIGNATURE | RESULT/RETURN | DESCRIPTION                                                                                                                                 |
| -------- | -------- | --------- | ------------- | ---------------------------------------------------- |
| UpdateTriggerDateTime | Method | STRING<br>STRING | STRING | Provides option for Configuring Specific date and Time for auto video trigger,and starts the timer.                              |
| UpdateTriggeringEvents | Method | UINT32 | STRING | Updates Auto video Trigger Events                                                                                                         |
| Date | Property | STRING | STRING | Requires date in `YYYY-MM-DD` format                                                                                                                      |
| Time | Property | STRING | STRING | Requires Time in `hh:mm:ss` [in `UTC` time standard]                                                                                                      |
| TriggeringEvents | Property | UINT32 | UINT32 | Holds the status of enabled or Disabled Event for Autovideo Trigger.<br> Each bit represents a specific Event<br> `1` : Enabled `0`: Disabled |


[Additional informations will be added on Requirment basis]

## License
Refer COPYING.AMI file
