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
    return $self->database($app)->execute('SELECT data FROM strings WHERE variant = ? AND name = ?', $variant, $string)->row;
}

sub getVariants {
    my $self = shift;
    my($app, $protocol) = @_;
    return $self->database($app)->execute('SELECT id, quality, type, encoding, charset, language FROM stringVariants WHERE protocol = ?', $protocol)->rows;
}

sub getDescribedVariants {
    my $self = shift;
    my($app) = @_;
    my %result = ();
    foreach my $variant ($self->database($app)->execute('SELECT id, name, protocol, quality, type, encoding, charset, language, description, translator FROM stringVariants')->rows) {
        $result{$variant->[0]} = {
            'name' => $variant->[1],
            'protocol' => $variant->[1],
            'quality' => $variant->[2],
            'type' => $variant->[3],
            'encoding' => $variant->[4],
            'charset' => $variant->[5],
            'language' => $variant->[6],
            'description' => $variant->[7],
            'translator' => $variant->[8],
        };
    }
    return %result;
}

sub getVariant {
    my $self = shift;
    my($app, $id) = @_;
    return $self->database($app)->execute('SELECT name, protocol, quality, type, encoding, charset, language, description, translator FROM stringVariants WHERE id = ?', $id)->row;
}

sub getVariantStrings {
    my $self = shift;
    my($app, $variant) = @_;
    my %result = ();
    foreach my $string ($self->database($app)->execute('SELECT name, data FROM strings WHERE variant = ?', $variant)->rows) {
        $result{$string->[0]} = $string->[1];
    }
    return %result;
}

sub getStringVariants {
    my $self = shift;
    my($app, $string) = @_;
    my %result = ();
    foreach my $variant ($self->database($app)->execute('SELECT variant, data FROM strings WHERE name = ?', $string)->rows) {
        $result{$variant->[0]} = $variant->[1];
    }
    return %result;
}

sub setVariant {
    my $self = shift;
    my($app, $id, $name, $protocol, $quality, $type, $encoding, $charset, $language, $description, $translator) = @_;
    if (defined($id)) {
        $self->database($app)->execute('UPDATE stringVariants SET name=?, protocol=?, quality=?, type=?, encoding=?, charset=?, language=?, description=?, translator=? WHERE id = ?', $name, $protocol, $quality, $type, $encoding, $charset, $language, $description, $translator, $id);
    } else {
        return $self->database($app)->execute('INSERT INTO stringVariants SET name=?, protocol=?, quality=?, type=?, encoding=?, charset=?, language=?, description=?, translator=?', $name, $protocol, $quality, $type, $encoding, $charset, $language, $description, $translator)->MySQLID;
    }
}

sub setString {
    my $self = shift;
    my($app, $variant, $string, $data) = @_;
    if ((defined($data)) and (length($data) > 0)) {
        $self->database($app)->execute('REPLACE INTO stringVariants SET variant=?, string=?, data=?', $variant, $string, $data);
    } else {
        $self->database($app)->execute('DELETE FROM stringVariants WHERE variant = ? AND string = ?', $variant, $string);
    }
}

sub setupInstall {
    my $self = shift;
    my($app) = @_;
    my $helper = $self->helper($app);
    $self->dump(9, 'about to configure string data source...');
    if (not $helper->tableExists($app, $self->database($app), 'stringVariants')) {
        $self->debug('going to create \'stringVariants\' table');
        $app->output->setupProgress('strings data source (creating stringVariants database)');
        $self->database($app)->execute('
            CREATE TABLE stringVariants (
                                         id integer unsigned auto_increment NOT NULL PRIMARY KEY,
                                         name varchar(255) NOT NULL,
                                         protocol varchar(255) NOT NULL,
                                         encoding varchar(255),
                                         type varchar(255) NOT NULL,
                                         charset varchar(255),
                                         language varchar(255) NOT NULL,
                                         quality float NOT NULL default 1.0,
                                         description text,
                                         translator varchar(255),
                                         UNIQUE KEY (name)
                                         )
        ');
    } else {
        # check its schema is up to date
    }
    if (not $helper->tableExists($app, $self->database($app), 'strings')) {
        $self->debug('going to create \'strings\' table');
        $app->output->setupProgress('strings data source (creating strings database)');
        $self->database($app)->execute('
            CREATE TABLE strings (
                                  variant integer unsigned NOT NULL,
                                  name varchar(32) NOT NULL,
                                  data text,
                                  PRIMARY KEY (variant, name)
                                  )
        ');
    } else {
        # check its schema is up to date
    }
    $self->dump(9, 'done configuring string data source');
    return;
}
