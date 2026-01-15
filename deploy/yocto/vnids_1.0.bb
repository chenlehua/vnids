# VNIDS Yocto Recipe
# BitBake recipe for building VNIDS
#
# Copyright (c) 2026 VNIDS Authors
# SPDX-License-Identifier: Apache-2.0

SUMMARY = "Vehicle Network Intrusion Detection System"
DESCRIPTION = "VNIDS provides network-based intrusion detection for vehicle networks using Suricata"
HOMEPAGE = "https://vnids.io"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=apache-license-hash"

DEPENDS = "sqlite3 cjson"
RDEPENDS_${PN} = "suricata"

SRC_URI = "git://github.com/vnids/vnids.git;branch=main;protocol=https"
SRCREV = "${AUTOREV}"

S = "${WORKDIR}/git"

inherit cmake systemd

SYSTEMD_SERVICE_${PN} = "vnidsd.service"

EXTRA_OECMAKE = "-DBUILD_TESTS=OFF"

do_install_append() {
    # Install configuration
    install -d ${D}${sysconfdir}/vnids
    install -d ${D}${sysconfdir}/vnids/suricata
    install -d ${D}${sysconfdir}/vnids/rules

    install -m 0640 ${S}/deploy/common/vnidsd.conf.example ${D}${sysconfdir}/vnids/vnidsd.conf
    install -m 0640 ${S}/suricata/config/suricata.yaml ${D}${sysconfdir}/vnids/suricata/

    # Install rules
    install -m 0644 ${S}/suricata/rules/*.rules ${D}${sysconfdir}/vnids/rules/
    install -m 0644 ${S}/suricata/rules/*.config ${D}${sysconfdir}/vnids/rules/

    # Install systemd service
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${S}/deploy/common/vnidsd.service ${D}${systemd_system_unitdir}/

    # Create runtime directories
    install -d ${D}${localstatedir}/lib/vnids
    install -d ${D}${localstatedir}/log/vnids
    install -d ${D}${localstatedir}/log/vnids/suricata
    install -d ${D}${localstatedir}/run/vnids
}

FILES_${PN} += " \
    ${sysconfdir}/vnids \
    ${localstatedir}/lib/vnids \
    ${localstatedir}/log/vnids \
    ${localstatedir}/run/vnids \
"

CONFFILES_${PN} = "${sysconfdir}/vnids/vnidsd.conf"
