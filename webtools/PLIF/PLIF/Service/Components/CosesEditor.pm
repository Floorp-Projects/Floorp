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

package PLIF::Service::Components::CosesEditor;
use strict;
use vars qw(@ISA);
use PLIF::Service;
@ISA = qw(PLIF::Service);
1;

# XXX Add more fine grained control over rights (this would have the
# side-effect of removing the redundancy in each of the cmdXXX calls
# below, which would be nice...)

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'component.cosesEditor' or
            $service eq 'dispatcher.commands' or
            $service eq 'dispatcher.output.generic' or
            $service eq 'dispatcher.output' or
            $service eq 'setup.install' or
            $class->SUPER::provides($service));
}

__DATA__

# dispatcher.commands
sub cmdCosesEditor {
    # warning: this is also called from other methods below
    my $self = shift;
    my($app) = @_;
    my $user = $app->getService('user.login')->hasRight($app, 'cosesEditor');
    if (defined($user)) {
        my $variants = $self->getDescribedVariants($app);
        my $variantsSortColumn = $app->input->getArgument('cosesEditor.variantsSortColumn');
        my $strings = $self->getExpectedStrings($app);
        my $stringsSortColumn = $app->input->getArgument('cosesEditor.stringsSortColumn');
        # if (defined($user)) {
        $user->setting(\$variantsSortColumn, 'cosesEditor.index.variantsSortColumn');
        $user->setting(\$stringsSortColumn, 'cosesEditor.index.stringsSortColumn');
        # }
        $app->output->cosesEditorIndex($variants, $variantsSortColumn, $strings, $stringsSortColumn);
    } # else, user has been notified
}

# dispatcher.commands
sub cmdCosesVariantAdd {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getService('user.login')->hasRight($app, 'cosesEditor');
    if (defined($user)) {
        my $protocol = $app->input->getArgument('cosesEditor.variantProtocol');
        my @data = ('', $protocol, 1.0, '', '', '', '', '', '');
        my $id = $app->getService('dataSource.strings.customised')->setVariant($app, undef, @data);
        my $expectedStrings = $self->getExpectedStrings($app, $protocol);
        $app->output->cosesEditorVariant($id, @data, $expectedStrings, {});
    } # else, user has been notified
}

# dispatcher.commands
sub cmdCosesVariantEdit {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getService('user.login')->hasRight($app, 'cosesEditor');
    if (defined($user)) {
        my $id = $app->input->getArgument('cosesEditor.variantID');
        my $dataSource = $app->getService('dataSource.strings.customised');
        my @data = $dataSource->getVariant($app, $id);
        my $expectedStrings = $self->getExpectedStrings($app, $data[1]);
        my $variantStrings = \$dataSource->getVariantStrings($app, $id);
        $app->output->cosesEditorVariant($id, @data, $expectedStrings, $variantStrings);
    } # else, user has been notified
}

# dispatcher.commands
sub cmdCosesVariantAddString {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getService('user.login')->hasRight($app, 'cosesEditor');
    if (defined($user)) {
        my($id, $data, $variantStrings) = $self->getVariantEditorArguments($app);
        my $expectedStrings = $self->getExpectedStrings($app, $data->[1]);
        $app->output->cosesEditorVariant($id, @$data, $expectedStrings, $variantStrings);
    } # else, user has been notified
}

# dispatcher.commands
sub cmdCosesVariantCommit {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getService('user.login')->hasRight($app, 'cosesEditor');
    if (defined($user)) {
        my($id, $data, $variantStrings) = $self->getVariantEditorArguments($app);
        my $dataSource = $app->getService('dataSource.strings.customised');
        $dataSource->setVariant($app, $id, @$data);
        foreach my $string (keys(%$variantStrings)) {
            $dataSource->setString($app, $id, $string, @{$variantStrings->{$string}});
        }
        $self->cmdCosesEditor($app);
    } # else, user has been notified
}

# dispatcher.commands
sub cmdCosesStringEdit {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getService('user.login')->hasRight($app, 'cosesEditor');
    if (defined($user)) {
        my $id = $app->input->getArgument('cosesEditor.stringID');
        my $strings = $self->getExpectedStrings($app);
        my $expectedVariants = $self->getDescribedVariants($app, $id);
        my $stringVariants = \$app->getService('dataSource.strings.customised')->getStringVariants($app, $id);
        $app->output->cosesEditorString($id, $strings->{$id}, $expectedVariants, $stringVariants);
    } # else, user has been notified
}

# dispatcher.commands
sub cmdCosesStringCommit {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getService('user.login')->hasRight($app, 'cosesEditor');
    if (defined($user)) {
        my $input = $app->input;
        my $id = $input->getArgument('cosesEditor.stringID');
        my %variants;
        my $index = 0;
        while (defined(my $name = $input->getArgument('cosesEditor.stringVariant.$index.name'))) {
            $variants{$name} = [$input->getArgument('cosesEditor.stringVariant.$index.type'),
                                $input->getArgument('cosesEditor.stringVariant.$index.version'),
                                $input->getArgument('cosesEditor.stringVariant.$index.value')];
            $index += 1;
        }
        my $dataSource = $app->getService('dataSource.strings.customised');
        foreach my $variant (keys(%variants)) {
            $dataSource->setString($app, $variant, $id, @{$variants{$variant}});
        }
        $self->cmdCosesEditor($app);
    } # else, user has been notified
}

# dispatcher.commands
sub cmdCosesVariantExport {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getService('user.login')->hasRight($app, 'cosesEditor');
    if (defined($user)) {
        # get data
        my $id = $app->input->getArgument('cosesEditor.variantID');
        my $dataSource = $app->getService('dataSource.strings.customised');
        my @data = $dataSource->getVariant($app, $id);
        my %strings = $dataSource->getVariantStrings($app, $id);

        # serialise variant
        my $XML = $app->getService('service.xml');
        # note. This namespace is certainly not set in stone. Please make better suggestions. XXX
        my $result = '<variant xmlns="http://bugzilla.mozilla.org/variant/1"';
        foreach my $name (qw(protocol quality type encoding charset language description translator)) {
            my $value = $XML->escape(shift(@data));
            $result .=   "\n         $name=\"$value\"";
        }
        $result .= ">\n";
        foreach my $string (keys(%strings)) {
            my $name = $XML->escape($string);
            my $type = $XML->escape($strings{$string}->[0]);
            my $version = $XML->escape($strings{$string}->[1]);
            my $value = $XML->escape($strings{$string}->[2]);
            $result.= "  <string name=\"$name\" type=\"$type\" version=\"$version\">$value</string>\n";
        }
        $result .= '</variant>';
        $app->output->cosesEditorExport($id, $result);
    } # else, user has been notified
}

# dispatcher.commands
sub cmdCosesVariantImport {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getService('user.login')->hasRight($app, 'cosesEditor');
    if (defined($user)) {
        # get data
        my $file = $app->input->getArgument('cosesEditor.importData');

        # parse data
        my $XML = $app->getService('service.xml');
        my $data = {
            'depth' => 0,
            'name' => undef,
            'type' => undef,
            'version' => undef,
            'variant' => [], # same at all scopes (because walkNesting() is not a deep copy)
            'strings' => {}, # same at all scopes (because walkNesting() is not a deep copy)
        };
        $XML->walk($self, $XML->parse($file), $data);

        # add data
        my $dataSource = $app->getService('dataSource.strings.customised');
        my $id = $dataSource->setVariant($app, undef, @{$data->{'variant'}});
        foreach my $string (keys(%{$data->{'strings'}})) {
            $dataSource->setString($app, $id, $string, @{$data->{'strings'}->{$string}});
        }

        # display data
        my %expectedStrings = $self->getExpectedStrings($app);
        my %variantStrings = $dataSource->getVariantStrings($app, $id);
        $app->output->cosesEditorVariant($id, @{$data->{'variant'}}, \%expectedStrings, \%variantStrings);
    } # else, user has been notified
}

# service.xml.sink
sub walkElement {
    my $self = shift;
    my($tagName, $attributes, $tree, $data) = @_;
    if ($tagName eq '{http://bugzilla.mozilla.org/variant/1}variant') {
        if ($data->{'depth'} == 0) {
            foreach my $name (qw(protocol quality type encoding charset language description translator)) {
                if (exists($attributes->{$name})) {
                    push(@{$data->{'variant'}}, $attributes->{$name});
                } else {
                    $self->error(1, "invalid variant document format - missing attribute '$name' on <variant>");
                }
            }
        } else {
            $self->error(1, 'invalid variant document format - <variant> not root element');
        }
    } elsif ($tagName eq '{http://bugzilla.mozilla.org/variant/1}string') {
        if ($data->{'depth'} == 1) {
            foreach my $name (qw(name type version)) {
                if (exists($attributes->{$name})) {
                    $data->{$name} = $attributes->{$name};
                } else {
                    $self->error(1, "invalid variant document format - missing attribute '$name' on <string>");
                }
            }
        } else {
            $self->error(1, 'invalid variant document format - <string> not child of <variant>');
        }
    } else {
        $self->error(1, "invalid variant document format - unexpected tag <$tagName>");
    }
    $data->{'depth'} += 1;
}

# service.xml.sink
sub walkText {
    my $self = shift;
    my($text, $data) = @_;
    if (defined($data->{'string'})) {
        $data->{'strings'}->{$data->{'name'}} = [$data->{'type'}, $data->{'version'}, $text];
    } elsif ($text !~ /^\w*$/o) {
        $self->error(1, "invalid variant document format - unexpected text");
    }
}

# service.xml.sink
sub walkNesting {
    my $self = shift;
    my($data) = @_;
    my %data = %$data; # copy first level of data hash
    return \%data; # return reference to copy
}


# dispatcher.output.generic
sub outputCosesEditorIndex {
    my $self = shift;
    my($app, $output, $variants, $variantsSortColumn, $strings, $stringsSortColumn) = @_;
    $output->output('cosesEditor.index', {
        'variants' => $variants,
        'variantsSortColumn' => $variantsSortColumn,
        'strings' => $strings,
        'stringsSortcolumn' => $stringsSortColumn,
    });
}

# dispatcher.output.generic
sub outputCosesEditorVariant {
    my $self = shift;
    my($app, $output, $variant, $protocol, $quality, $type, $encoding, $charset, $language, $description, $translator, $expectedStrings, $variantStrings) = @_;
    $output->output('cosesEditor.variant', {
        'variant' => $variant,
        'protocol' => $protocol,        
        'quality' => $quality,        
        'type' => $type,        
        'encoding' => $encoding,        
        'charset' => $charset,        
        'language' => $language,        
        'description' => $description,        
        'translator' => $translator,        
        'expectedStrings' => $expectedStrings,
        'variantStrings' => $variantStrings,
    });
}

# dispatcher.output.generic
sub outputCosesEditorString {
    my $self = shift;
    my($app, $output, $string, $description, $expectedVariants, $stringVariants) = @_;
    $output->output('cosesEditor.string', {
        'string' => $string,
        'description' => $description,
        'expectedVariants' => $expectedVariants,
        'stringVariants' => $stringVariants,
    });
}

# dispatcher.output.generic
sub outputCosesEditorExport {
    my $self = shift;
    my($app, $output, $variant, $result) = @_;
    $output->output('cosesEditor.export', {
        'variant' => $variant,
        'output' => $result,
    });
}

# dispatcher.output
sub strings {
    return (
            'cosesEditor.index' => 'The COSES editor index. The variants hash (variant ID => hash with keys name, protocol, quality, type, encoding, charset, language, description, and translator) should be sorted by the variantsSortColumn, and the strings hash (name=>description) should be sorted by the stringsSortColumn (these are typically set by the cosesEditor.variantsSortColumn and cosesEditor.stringsSortColumn arguments). Typical commands that this should lead to: cosesVariantAdd (optional cosesEditor.variantProtocol), cosesVariantEdit (cosesEditor.variantID), cosesStringEdit (cosesEditor.stringID), cosesVariantExport (cosesEditor.variantID), cosesVariantImport (cosesEditor.importData, the contents of an XML file)',
            'cosesEditor.variant' => 'The COSES variant editor. The data hash contains: protocol, quality, type, encoding, charset, language, description and translator (hereon "the variant data"), variant, an expectedStrings hash (name=>description), and a variantStrings hash (name=>[type,version,value]). The two hashes are likely to overlap. Typical commands that this should lead to: cosesVariantCommit and cosesVariantAddString (cosesEditor.variantID, cosesEditor.variantX where X is each of the variant data, cosesEditor.variantString.N.name, cosesEditor.variantString.N.type, cosesEditor.variantString.N.version, and cosesEditor.variantString.N.value where N is a number from 0 to as high as required, and cosesEditor.string.new.name, cosesEditor.string.new.type, cosesEditor.string.new.version and cosesEditor.variantString.new.value)',
            'cosesEditor.string' => 'The COSES string editor. The name of the string being edited and its description are in string and description. The expectedVariants contains a list of all variants (variant ID => hash with keys name, protocol, quality, type, encoding, charset, language, description, and translator), and stringVariants hosts the currently set strings (variant=>value). The main command that this should lead to is: cosesStringCommit (cosesEditor.stringID, cosesEditor.stringVariant.N.name, cosesEditor.stringVariant.N.type, cosesEditor.stringVariant.N.version and cosesEditor.stringVariant.N.value where N is a number from 0 to as high as required)',
            'cosesEditor.export' => 'The COSES variant export feature. variant holds the id of the variant, and output holds the XML representation of the variant.',
            );
}

# setup.install
# XXX at least part of this could also be implemented as a user field
# factory registerer hook -- does this matter?
sub setupInstall {
    my $self = shift;
    my($app) = @_;
    $self->dump(9, 'about to configure COSES editor...');
    my $fieldFactory = $app->getService('user.fieldFactory');
    $fieldFactory->registerSetting($app, 'cosesEditor.index.stringsSortColumn', 'string');
    $fieldFactory->registerSetting($app, 'cosesEditor.index.variantsSortColumn', 'string');
    my $userDataSource = $app->getService('dataSource.user');
    $userDataSource->addRight($app, 'cosesEditor');
    $self->dump(9, 'done configuring COSES editor');
    return;
}



# Internal Routines

sub getVariantEditorArguments {
    my $self = shift;
    my($app) = @_;
    my $input = $app->input;
    my $id = $input->getArgument('cosesEditor.variantID');
    my @data = ();
    foreach my $argument (qw(name protocol quality type encoding charset language description translator)) {
        push(@data, $input->getArgument('cosesEditor.variant\u$argument'));
    }
    my %variantStrings = ();
    my $index = 0;
    while (defined(my $name = $input->getArgument('cosesEditor.variantString.$index.name'))) {
        $variantStrings{$name} = [$input->getArgument('cosesEditor.variantString.$index.type'),
                                  $input->getArgument('cosesEditor.variantString.$index.version'),
                                  $input->getArgument('cosesEditor.variantString.$index.value')];
        $index += 1;
    }
    my $newName = $input->getArgument('cosesEditorVariantStringNewName');
    if ((defined($newName)) and ($newName ne '')) {
        $variantStrings{$newName} = [$input->getArgument('cosesEditor.variantString.new.type'),
                                     $input->getArgument('cosesEditor.variantString.new.version'),
                                     $input->getArgument('cosesEditor.variantString.new.value')];
    }
    return ($id, \@data, \%variantStrings);
}

sub getExpectedStrings {
    my $self = shift;
    my($app, $protocol) = @_;
    my $strings = $app->getCollectingServiceList('dispatcher.output')->strings();
    if (defined($protocol)) {
        my $defaults = $app->getSelectingService('dataSource.strings.default');
        foreach my $string (keys(%$strings)) {
            my $args = {
                'app' => $app,
                'protocol' => $protocol,
                'name' => $string,
            };
            $defaults->getDefaultString($args);
            $strings->{$string} = {
                'description' => $strings->{$string},
                'default' => $args->{'string'},
            };
        }
    }
    return $strings;
}

sub getDescribedVariants {
    my $self = shift;
    my($app, $string) = @_;
    my $variants = \$app->getService('dataSource.strings.customised')->getDescribedVariants($app);
    if (defined($string)) {
        my $defaults = $app->getSelectingService('dataSource.strings.default');
        foreach my $variant (keys(%$variants)) {
            my $args = {
                'app' => $app,
                'protocol' => $variants->{$variant}->[1],
                'name' => $string,
            };
            $defaults->getDefaultString($args);
            push(@{$variants->{$variant}}, $args->{'string'});
        }
    }
    return $variants;
}
