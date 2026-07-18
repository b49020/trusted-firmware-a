Qualcomm Nord platform
=======================

Trusted Firmware-A (TF-A) platform port for the Qualcomm Nord SoC, part of
the Wildcat platform family (``plat/qti/wildcat/nord``). Nord has three
clusters of six cores each (18 cores total).

Unlike the other Qualcomm ports, the Nord port does not link against the
closed ``QTISECLIB`` blob: BL31 is built entirely from in-tree sources. The
power-sequencing pieces that ``QTISECLIB`` would normally provide are backed
by in-tree drivers (the PDC/RSC and cmd_db drivers) and a functional stub for
the remaining ``bl31qtilib``/sysini hooks.

Boot flow
---------

XBL loads and authenticates the BL2 (signed as the TZ image) and the FIP,
then enters BL2 at EL3. BL2 loads BL31, the BL32 payload (OP-TEE, under
``SPD=opteed``) and BL33 from the FIP, and hands over to BL31. BL31 brings up
the secondary cores via PSCI ``CPU_ON`` and starts the non-secure OS at EL2.

How to build
------------

Steps to build TF-A BL31 and BL2::

	$ make CROSS_COMPILE=aarch64-none-elf- PLAT=nord_qcs bl31 bl2

To build with an OP-TEE BL32 payload and a non-secure BL33 packed into a
FIP::

	$ make CROSS_COMPILE=aarch64-none-elf- PLAT=nord_qcs SPD=opteed \
	    BL32=<path-to-optee-bin> BL33=<path-to-os-bootloader-bin> \
	    bl31 bl2 fip all

Note that the ``bl2.elf`` generated here must be signed as the TZ image with
QTI signing involved, and the FIP must be placed where XBL expects it
(``PLAT_QTI_FIP_IOBASE``).

Status and caveats
------------------

This is an early bring-up port. The following points are interim and must be
reconciled with the authoritative Nord sources before relying on the platform
on production silicon:

- **CPU library.** Nord has no dedicated Oryon CPU library yet and reuses the
  Cortex-A73 library as a stand-in (registered against the Oryon-1 MIDR). The
  A73 errata and Spectre/CVE workarounds are disabled because their
  A73-specific reset sequences trap on Oryon. Oryon needs its own errata and
  Spectre story; this is left as future work.

- **Power sequencing.** The ``bl31qtilib``/sysini backing is a functional
  stub, not the authoritative Nord power-sequencing code. The PDC/RSC and
  cmd_db drivers are ported in-tree for the non-secure RPMh path.

- **Static memory protection.** The XPU static MPU partition tables are left
  empty (the authoritative Nord XPU map was not available); interrupts remain
  enabled. Populate the tables from the authoritative Nord XPU map before
  relying on static memory protection.

- **GIC grouping.** The Nord GIC-700 resets with ``IGRPMODR`` set, so the port
  explicitly regroups non-secure SPIs and PPIs/SGIs to Group-1NS and enables
  ``GICD_CTLR.EnableGrp1NS`` so the non-secure OS receives its interrupts.

--------------

*Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.*
