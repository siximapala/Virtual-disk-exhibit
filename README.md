Project consists of three programs designed for creating, mounting, and formatting virtual disks in VHDX format:
1. **CreateVHDX** - Create a virtual disk
2. **MontageVHD** - Mount the disk into the system
3. **FormatAndWrite** - Initialize and format the disk

### What is VHDX
VHDX (Virtual Hard Disk) is Microsoft's virtual disk format that enables the creation of virtual hard disks with support for modern features:
- Support for disks up to 64 TB
- Protection against data loss in case of power failure
- Optimization for high-performance storage systems

## Workflow

### 1. Creating a VHDX disk (CreateVHD.cpp)
#### Functionality:
- Creates a virtual disk of size 3 MB
- Saves in VHDX format
- Automatically creates required directories

#### How it works:
1. Uses the Windows API `CreateVirtualDisk`
2. Specifies the storage type `VIRTUAL_STORAGE_TYPE_DEVICE_VHDX`
3. Configures parameters:
   - Size: 3 MB
   - Default block size
   - Default sector size

### 2. Mounting the disk (MontageVHD.cpp)
#### Functionality:
- Attaches the VHDX file as a virtual disk
- Retrieves the disk's unique GUID
- Displays the list of disks via PowerShell

#### How it works:
1. Opens the disk with read access
2. Attaches permanently using `AttachVirtualDisk`
3. Retrieves the disk identifier
4. Outputs information about all disks

### 3. Formatting the disk (FormatAndWrite.cpp)
#### Functionality:
- Initializes the disk with MBR
- Creates a single partition
- Assigns a drive letter
- Formats to NTFS

#### How it works:
1. Checks disk size (minimum 3 MB)
2. Creates an MBR record with one partition
3. Assigns the first available letter (D-Z)
4. Formats via `SHFormatDrive`

## Installation
```bash
# Clone the repository
git clone https://github.com/siximapala/Virtual-disk-exhibit.git
cd VirtualDiskProject

# Create build directory
mkdir build
cd build

# CMake
cmake ..

# Build the project
cmake --build . --config Release
```
## Operation Sequence:
### 1. Create the disk
```
CreateVHDX "C:\path\to\disk.vhdx"
```
### 2. Mount the disk
```
MontageVHD "C:\path\to\disk.vhdx"
```
### 3. Format the disk
```
FormatAndWrite <disk-number>
```
```
# 1. Create the disk
CreateVHDX "C:\disks\test.vhdx"

# 2. Mount the disk
MontageVHD "C:\disks\test.vhdx"
# Output:
# Disk ID: 123E4567-E89B-12D3-A456-426614174000
#
# Checking disks via PowerShell...
# NUMBER  FRIENDLY NAME            SIZE
# 1       Microsoft Virtual Disk  3 MB

# 3. Format the disk (in this case disk number is 1)
FormatAndWrite 1
```
Requires administrator privileges to work with disks. It is also recommended to disable antivirus. VHDX disks are supported only on Windows 8 or later (10, 11).
