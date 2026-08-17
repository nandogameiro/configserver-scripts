#!/bin/sh
###############################################################################
# Copyright (C) 2006-2025 Jonathan Michaelson
#
# https://github.com/waytotheweb/scripts
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 3 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <https://www.gnu.org/licenses>.
###############################################################################
PATH=$PATH:/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin

# Refuse to run outside the unpacked source tree. The shebang rewrite below
# used to recurse over the whole working directory, so running this from the
# wrong place would rewrite unrelated files on the system.
if [ ! -f "cpanel/cmq.cgi" ] || [ ! -f "cmqversion.txt" ]; then
	echo "ERROR: run this installer from within the unpacked cmq source directory"
	exit 1
fi

#First replace:
if [ -e "/usr/local/cpanel/3rdparty/bin/perl" ]; then
    sed -i 's%^#\!/usr/bin/perl%#\!/usr/local/cpanel/3rdparty/bin/perl%' cpanel/cmq.cgi
fi

mkdir -p /etc/cmq
chmod 700 /etc/cmq

mkdir -p /usr/local/cpanel/whostmgr/docroot/cgi/configserver
chmod 700 /usr/local/cpanel/whostmgr/docroot/cgi/configserver
mkdir -p /usr/local/cpanel/whostmgr/docroot/cgi/configserver/cmq
chmod 700 /usr/local/cpanel/whostmgr/docroot/cgi/configserver/cmq

cp -avf cpanel/cmq.cgi /usr/local/cpanel/whostmgr/docroot/cgi/configserver/cmq.cgi
chmod -v 700 /usr/local/cpanel/whostmgr/docroot/cgi/configserver/cmq.cgi

cp -avf Modules /etc/cmq/
cp -avf cmqversion.txt /etc/cmq/
cp -avf downloadservers /etc/cmq/
cp -avf INSTALL.txt /etc/cmq/
cp -avf uninstall.sh /etc/cmq/
chmod 700 /etc/cmq/uninstall.sh

cp -avf cmq/ /usr/local/cpanel/whostmgr/docroot/cgi/configserver/
cp -avf cpanel/cmq.conf /etc/cmq/cmq.conf
cp -avf upgrade.sh /etc/cmq/upgrade.sh
chmod 700 /etc/cmq/upgrade.sh
cp -af cmq/cmq.png /usr/local/cpanel/whostmgr/docroot/addon_plugins/
cp -af cpanel/cmq.tmpl /usr/local/cpanel/whostmgr/docroot/templates/

/usr/local/cpanel/bin/register_appconfig /etc/cmq/cmq.conf

/bin/rm -f /usr/local/cpanel/whostmgr/docroot/cgi/addon_cmq.cgi
/bin/rm -f /usr/local/cpanel/whostmgr/docroot/cgi/cmqversion.txt
/bin/rm -Rf /usr/local/cpanel/whostmgr/docroot/cgi/cmq

#Second replace
if [ -e "/usr/local/cpanel/3rdparty/bin/perl" ]; then
	sed -i 's%^#\!/usr/local/cpanel/3rdparty/bin/perl%#\!/usr/bin/perl%' cpanel/cmq.cgi
fi

echo "ConfigServer Mail Queues has been installed."
exit
