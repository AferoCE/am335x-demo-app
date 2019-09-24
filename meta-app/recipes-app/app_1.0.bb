# Copyright (C) 2017 Afero, Inc. All rights reserved

DESCRIPTION = "Afero Attribute Deamon"
SECTION = "examples"
DEPENDS = "libevent af-util af-ipc attrd"
# DEPENDS = "libevent af-util af-ipc af-edge attrd"
LICENSE = "CLOSED"
LIC_FILES_CHKSUM = ""

# FILESEXTRAPATHS_prepend := "${THISDIR}/${PN}-${PV}:"
OECMAKE_GENERATOR = "Unix Makefiles"
# SRC_URI += " file://app.service"
# SRC_URI += " file://Makefile \ 
#	file://test.c"

inherit externalsrc systemd

EXTERNALSRC = "${TOPDIR}/../af-app"

SYSTEMD_PACKAGES = "${PN}"

PARALLEL_MAKE = ""


do_compile(){
#
# Change directories into the source code directory
#
	cd ${S}
#
# Run the Makefile that you find there.
#
	oe_runmake
}

do_install_append() {
#
# Make sure the directory you want to put the file in exists by creating it.
# If it already exists, this will do nothing.
#
    install -d ${D}/usr/bin
#
# Next, move the file, app, from where it's been built and deposited
#  by the do compile and set the permissions on it to rwxr-wr-x
#
    install -m 755 ${EXTERNALSRC}/app ${D}/usr/bin
}
