# SDEI Prototype Demonstration

Contents:

1.  [Introduction](#introduction)
2.  [Components and Status](#components-and-status)
3.  [Instructions](#instructions)


## Introduction
This ARM Trusted Firmware branch can be used in conjunction with other software
components described below to demonstrate the use of physical SDEI in delegating
a RAS error from ARM Trusted Firmware to the Normal World.

The demonstration works on the ARMv8 Architecture FVP which is available from
the [ARM FVP website]. Support for Virtualisation Host Extension (introduced in
ARMv8.1) must be enabled in the FVP. It is assumed that the user is familiar
with the following hardware and software components and concepts. This is not an
exhaustive list.

1.  ARMv8 Architecture FVP
2.  Reliability, Availability and Serviceability (RAS)
3.  ACPI & UEFI
4.  APEI (ACPI Platform Error Interface)
5.  Software Delegated Exception Interface (SDEI)
6.  ARM Trusted Firmware (ARM TF)
7.  EDK2
8.  Linux
9.  ARMv8-A Architecture

In this demonstration, ARM TF delegates a fake memory-corruption event using
APEI and SDEI to Linux at EL2. The error event is handled by the SDEI driver in
Linux which delegates it to the APEI driver. The APEI driver then signals the
relevant user space process.

The fake memory-corruption event is generated from Linux userspace by writing to
a debugfs file. A fake CPER record is created when this file is written and a
SP804 timer on the FVP is programmed to deliver the event to ARM Trusted
Firmware.

EDK2 has been modified to provide the HEST and SDEI ACPI tables.

The Virtual Software Delegated Exception Interface is not demonstrated with this
software stack. It is possible to do so with KVM and its Guest VMs. For the sake
of simplicity those instructions have not been included. They can be provided as
a add-on once this demonstration is successfully recreated.


## Components and status
The maturity of the implementation of various software components used in this
demonstration varies. None of the components have been merged in their
respective upstream repositories and are not suitable for deployment on a
production system. Some components have a very limited lifetime as they have
been specifically developed to enable this demonstration and are not relevant on
production systems.

The components, their status and locations are listed below

1.  [ARM Trusted Firmware SDEI Prototype branch] : Early Development
2.  [Linux] : Pending further testing and review, most of this code will be merged upstream.
3.  [ACPICA] : Pending further testing and review, the patch to add the SDEI table will be upstreamed
4.  [EDK2] & [OpenPlatformPkg]: Temporary changes to generate a APEI HEST and
allocate memory for CPERs. The HEST is rewritten by the MangleHEST application
prior to booting Linux. This would be done in co-ordination with the secure
world on a production system.The GHES Error Status Address should point into an
area of type ACPI_NVS reserved in the EFI memory map. This isn't being done
right now. So none of these changes will be upstreamed.
5. print_my_pa application - This helper program is a part of the [ACPICA]
branch and prints the physical address of a buffer. The fake memory corruption
event is associated with this address. The helper program then waits for the
error to be delivered to it. This application is easy to victimise with ras
events and was developed solely for that purpose.


## Instructions
This section describes the instructions to recreate the demonstration on the ARMv8-A Architecture FVP. These are not step-by-step instructions and should be adapted to the target development environment.

*  Retrieve the relevant versions of ARM Trusted Firmware, UEFI/EDK2, Linux and ACPICA :
```
git clone --recursive https://github.com/james-morse-arm/acpica.git -b sdei+demo
```

*  The build scripts described below may need modification prior to invocation. Please understand their contents before trying to run them.
```
> cd ras_demo
> bash build.sh
> sudo bash build_disk_image.sh
> bash start_model.sh
```

1.  build.sh: This scripts builds ACPICA first, so that UEFI/EDK2 can use the
    patched IASL compiler to generate the SDEI table. Next EDK2 and ATF are
    built, a kernel-config is generated and the kernel built, as is
    'print_my_pa'. Finally build.sh downloads a filesystem image from Linaro for
    use by the next script.

2.  build_disk_image.sh: This needs to run as root to mount filesystems, it
    creates a 10G disk image with an EFI partition and a Linux root
    filesystem. It copies the EFI application 'MangleHEST' and the kernel Image
    into the EFI partition and unpacks the Linaro filesystem image into the root
    filesystem. It creates a startup.nsh for edk2 to run which uses MangleHEST
    to allocate GHES Error-Status-Address memory in the UEFI memory map and
    update the HEST table, then it boots the kernel.

3.  start_model.sh: This script should be modified as the presence of the ARMv8
    Architecture FVP on the target development platform. The model parameters
    are present in the .param file. Once started, the model runs ARM TF and
    EDK2. It then boots Linux. There will be one window for kernel
    serial-console output and another for debug messages from ARM Trusted
    Firmware.

4.  Once Linux's user space has (finally) booted, log in as root, and run
    './print_my_pa &'. This will print a physical address. Echo this address
    into /sys/kernel/debug/sdei_ras_poison. This will generate fake CPER records
    for a parity error at this address, then start the sp804 to trigger an SDEI
    event. After 30 seconds the error will be delivered to the application and
    it will be killed.

```
root@linaro-developer:~# ./print_my_pa &
[1] 2841
Buffer Physical address: 0x8f5aee010
root@linaro-developer:~#
root@linaro-developer:~# echo 0x8f5aee010 > /sys/kernel/debug/sdei_ras_poison
[11047.079677] sdei: Timer running
root@linaro-developer:~# fg
./print_my_pa

[11077.080059] {1}[Hardware Error]: Hardware error from APEI Generic Hardware
Error Source: 1
[11077.080077] {1}[Hardware Error]: event severity: recoverable
[11077.080096] {1}[Hardware Error]:  Error 0, type: recoverable
[11077.080117] {1}[Hardware Error]:  fru_id: 1a32d511-8a47-4565-ab85-4d950653855c
[11077.080136] {1}[Hardware Error]:  fru_text: Your String Here
[11077.080154] {1}[Hardware Error]:   section_type: memory error
[11077.080175] {1}[Hardware Error]:   physical_address: 0x00000008f5aee010
[11077.080196] {1}[Hardware Error]:   physical_address_mask: 0xfffffffffffff000
[11077.080215] {1}[Hardware Error]:   error_type: 8, parity error
[11077.081071] Memory failure: 0x8f5aee: recovery action for dirty LRU page:
Recovered
[11077.081196] print_my_pa[2841]: unhandled level 3 translation fault (7) at
0x32b70fa3, esr 0x92000007
[11077.081277] pgd = ffff800879b03000
[11077.081332] [32b70fa3] *pgd=00000008f7a8a003, *pud=00000008f7bb8003,
*pmd=00000008f98b6003, *pte=000000008f5aee74
[11077.081490] CPU: 0 PID: 2841 Comm: print_my_pa Not tainted
4.12.0-rc1-g4487fb0470d9 #4
[11077.081564] Hardware name: (null) (DT)
[11077.081627] task: ffff800877840000 task.stack: ffff800877b90000
[11077.081695] PC is at 0x400b34
[11077.081748] LR is at 0x400b64
[11077.081809] pc : [<0000000000400b34>] lr : [<0000000000400b64>] pstate: 80000000
[11077.081881] sp : 0000fffff5108eb0
[11077.081934] x29: 0000fffff5108eb0 x28: 0000000000000000
[11077.082025] x27: 0000000000000000 x26: 0000000000000000
[11077.082115] x25: 0000000000000000 x24: 0000000000000000
[11077.082205] x23: 0000000000000000 x22: 0000000000000000
[11077.082296] x21: 00000000004007b0 x20: 0000000000000000
[11077.082387] x19: 0000000000000f93 x18: 0000fffff5108c10
[11077.082480] x17: 0000ffff97452d98 x16: 0000000000410f10
[11077.082573] x15: 0000ffff974f9588 x14: 0000ffff973b1a94
[11077.082664] x13: 0000000000000000 x12: 0000000000000024
[11077.082755] x11: 0000000000000008 x10: 00000000ffffffff
[11077.082846] x9 : 0000000000000000 x8 : 0000000000000040
[11077.082935] x7 : 0000000000000000 x6 : 0000000000000000
[11077.083285] x5 : 00000000fbad2887 x4 : 00000000ffffffff
[11077.083550] x3 : 0000000000000000 x2 : 0000ffff97452efc
[11077.083855] x1 : 0000000032b70010 x0 : 0000000032b70fa3
Bus error
```
The `unhandled level 3 translation fault` occurs because 'print_my_pa' fails to
handle the SIGBUS resulting from the memory hwpoison event. More complex
programs such as web browsers or Qemu/kvmtool will be able to recover from this
error. (e.g. by reloading a web page, or notifying the guest about the error).


- - - - - - - - - - - - - - - - - - - - - - - - - -

[ARM FVP website]:                            https://developer.arm.com/products/system-design/fixed-virtual-platforms
[ARM Trusted Firmware SDEI Prototype branch]: https://github.com/ARM-software/arm-trusted-firmware/tree/prototypes/sdei/rfc_v1
[Linux]:                                      http://www.linux-arm.org/git?p=linux-jm.git;a=shortlog;h=refs/heads/sdei/v1/demo
[ACPICA]:                                     https://github.com/james-morse-arm/acpica/tree/sdei
[EDK2]:                                       https://github.com/james-morse-arm/edk2-staging/tree/mangle-HEST
[OpenPlatformPkg]:                            https://git.linaro.org/uefi/OpenPlatformPkg.git/log/?h=mangle-HEST
