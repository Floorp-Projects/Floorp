# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil; -*-
#
# This file is MPL/GPL dual-licensed under the following terms:
# 
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and
# limitations under the License.
#
# The Original Code is PLIF 1.0.
# The Initial Developer of the Original Code is Ian Hickson.
#
# Alternatively, the contents of this file may be used under the terms
# of the GNU General Public License Version 2 or later (the "GPL"), in
# which case the provisions of the GPL are applicable instead of those
# above. If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the MPL, indicate your decision by
# deleting the provisions above and replace them with the notice and
# other provisions required by the GPL. If you do not delete the
# provisions above, a recipient may use your version of this file
# under either the MPL or the GPL.

package PLIF::DataSource::Strings::MySQL;
use strict;
use vars qw(@ISA);
use PLIF::DataSource::Strings;
@ISA = qw(PLIF::DataSource::Strings);
1;

sub databaseType {
    return qw(mysql);
}

sub getString { 
    my $self = shift;
    my($app, $variant, $string) = @_;    
    return $self->database($app)->execute("SELECT data FROM strings WHERE variant = ? string = ?", $variant, $string)->rows;
}

sub getVariants {
    my $self = shift;
    my($app, $protocol) = @_;
    return $self->database($app)->execute("SELECT id, quality, type, encoding, charset, language FROM variants WHERE protocol = ?", $protocol)->rows;
}

sub setupInstall {
    my $self = shift;
    my($app) = @_;
    my $helper = $self->helper($app);
    if (not $helper->tableExists($app, $self->database($app), 'stringVariants')) {
        $self->database($app)->execute('
            CREATE TABLE stringVariants (
                                         id integer unsigned auto_increment not null primary key,
                                         name varchar(255) not null,
                                         protocol varchar(255) not null,
                                         encoding varchar(255),
                                         type varchar(255) not null,
                                         charset varchar(255),
                                         language varchar(255) not null,
                                         quality float not null default 1.0,
                                         description text,
                                         translator varchar(255),
                                         unique index (name)
                                         );
        ');
    } else {
        # check its schema is up to date
    }
    if (not $helper->tableExists($app, $self->database($app), 'strings')) {
        $self->database($app)->execute('
            CREATE TABLE strings (
                                  variant integer unsigned not null,
                                  name varchar(32) not null,
                                  data text,
                                  primary key (variant, name)
                                  );
        ');
    } else {
        # check its schema is up to date
    }
    return;
}
