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
            $service eq 'dataSource.strings.default' or
            $service eq 'setup.install' or
            $class->SUPER::provides($service));
}

# dispatcher.commands
sub cmdCosesEditor {
    # warning: this is also called from other methods below
    my $self = shift;
    my($app) = @_;
    my $user = $app->getService('user.login')->hasRight($app, 'cosesEditor');
    if (defined($user)) {
        my %variants = $app->getService('dataSource.strings')->getDescribedVariants();
        my $variantsSortColumn = $app->input->getArgument('cosesEditorVariantsSortColumn');
        my %strings = @{$app->getCollectingServiceList('dispatcher.output')->strings};
        my $stringsSortColumn = $app->input->getArgument('cosesEditorStringsSortColumn');
        # if (defined($user)) {
        $user->setting(\$variantsSortColumn, 'cosesEditor.index.variantsSortColumn');
        $user->setting(\$stringsSortColumn, 'cosesEditor.index.stringsSortColumn');
        # }
        $app->output->cosesEditorIndex(\%variants, $variantsSortColumn, \%strings, $stringsSortColumn);
    } # else, user has been notified
}

# dispatcher.commands
sub cmdCosesVariantAdd {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getService('user.login')->hasRight($app, 'cosesEditor');
    if (defined($user)) {
        my @data = ('', '', 1.0, '', '', '', '', '', '');
        my $id = $app->getService('dataSource.strings')->setVariant($app, undef, @data);
        my %expectedStrings = @{$app->getCollectingServiceList('dispatcher.output')->strings};
        $app->output->cosesEditorVariant($id, @data, \%expectedStrings, {});
    } # else, user has been notified
}

# dispatcher.commands
sub cmdCosesVariantEdit {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getService('user.login')->hasRight($app, 'cosesEditor');
    if (defined($user)) {
        my $id = $app->input->getArgument('cosesEditorVariantID');
        my $dataSource = $app->getService('dataSource.strings');
        my @data = $dataSource->getVariant($app, $id);
        my %expectedStrings = @{$app->getCollectingServiceList('dispatcher.output')->strings};
        my %variantStrings = $dataSource->getVariantStrings($app, $id);
        $app->output->cosesEditorVariant($id, @data, \%expectedStrings, \%variantStrings);
    } # else, user has been notified
}

# dispatcher.commands
sub cmdCosesVariantAddString {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getService('user.login')->hasRight($app, 'cosesEditor');
    if (defined($user)) {
        my %expectedStrings = @{$app->getCollectingServiceList('dispatcher.output')->strings};
        my($id, $data, $variantStrings) = $self->getVariantEditorArguments($app);
        $app->output->cosesEditorVariant($id, @$data, \%expectedStrings, $variantStrings);
    } # else, user has been notified
}

# dispatcher.commands
sub cmdCosesVariantCommit {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getService('user.login')->hasRight($app, 'cosesEditor');
    if (defined($user)) {
        my($id, $data, $variantStrings) = $self->getVariantEditorArguments($app);
        my $dataSource = $app->getService('dataSource.strings');
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
        my $id = $app->input->getArgument('cosesEditorStringID');
        my %strings = @{$app->getCollectingServiceList('dispatcher.output')->strings};
        my $dataSource = $app->getService('dataSource.strings');
        my %expectedVariants = $dataSource->getDescribedVariants();    
        my %stringVariants = $dataSource->getStringVariants($app, $id);
        $app->output->cosesEditorString($id, $strings{$id}, \%expectedVariants, \%stringVariants);
    } # else, user has been notified
}

# dispatcher.commands
sub cmdCosesStringCommit {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getService('user.login')->hasRight($app, 'cosesEditor');
    if (defined($user)) {
        my $input = $app->input;
        my $id = $input->getArgument('cosesEditorStringID');
        my %variants = ();
        my $index = 0;
        while (defined(my $name = $input->getArgument('cosesEditorStringVariant${index}Name'))) {
            $variants{$name} = [$input->getArgument('cosesEditorStringVariant${index}Type'), 
                                $input->getArgument('cosesEditorStringVariant${index}Value')];
            $index += 1;
        }
        my $dataSource = $app->getService('dataSource.strings');
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
        my $id = $app->input->getArgument('cosesEditorVariantID');
        my $dataSource = $app->getService('dataSource.strings');
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
            my $value = $XML->escape($strings{$string}->[1]);
            $result.= "  <string name=\"$name\" type=\"$type\">$value</string>\n";
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
        my $file = $app->input->getArgument('cosesEditorImportData');

        # parse data
        my $XML = $app->getService('service.xml');
        my $data = {
            'depth' => 0,
            'string' => undef,
            'type' => undef,
            'variant' => [], # same at all scopes (because walkNesting() is not a deep copy)
            'strings' => {}, # same at all scopes (because walkNesting() is not a deep copy)
        };
        $XML->walk($self, $XML->parse($file), $data);

        # add data
        my $dataSource = $app->getService('dataSource.strings');
        my $id = $dataSource->setVariant($app, undef, @{$data->{'variant'}});
        foreach my $string (keys(%{$data->{'strings'}})) {
            $dataSource->setString($app, $id, $string, @{$data->{'strings'}->{$string}});
        }

        # display data
        my %expectedStrings = @{$app->getCollectingServiceList('dispatcher.output')->strings};
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
            if (exists($attributes->{'name'})) {
                $data->{'string'} = $attributes->{'name'};
                $data->{'type'} = $attributes->{'type'};
            } else {
                $self->error(1, 'invalid variant document format - missing attribute \'name\' on <string>');
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
        $data->{'strings'}->{$data->{'string'}} = [$data->{'type'}, $text];
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
            'cosesEditor.index' => 'The COSES editor index. The data.variants hash (variant ID => hash with keys name, protocol, quality, type, encoding, charset, language, description, and translator) should be sorted by the data.variantsSortColumn, and the data.strings hash (name=>description) should be sorted by the data.stringsSortColumn (these are typically set by the cosesEditorVariantsSortColumn and cosesEditorStringsSortColumn arguments). Typical commands that this should lead to: cosesVariantAdd (no arguments), cosesVariantEdit (cosesEditorVariantID), cosesStringEdit (cosesEditorStringID), cosesVariantExport (cosesEditorVariantID), cosesVariantImport (cosesEditorImportData, the contents of an XML file)',
            'cosesEditor.variant' => 'The COSES variant editor. The data hash contains: protocol, quality, type, encoding, charset, language, description and translator (hereon "the variant data"), variant, an expectedStrings hash (name=>description), and a variantStrings hash (name=>[type,value]). The two hashes are likely to overlap. Typical commands that this should lead to: cosesVariantCommit and cosesVariantAddString (cosesEditorVariantID, cosesEditorVariantX where X is each of the variant data, cosesEditorVariantStringNName, cosesEditorVariantStringNType and cosesEditorVariantStringNValue where N is a number from 0 to as high as required, and cosesEditorStringNewName, cosesEditorStringNewType and cosesEditorVariantStringNewValue)',
            'cosesEditor.string' => 'The COSES string editor. The name of the string being edited and its description are in data.string and data.description. The data.expectedVariants contains a list of all variants (variant ID => hash with keys name, protocol, quality, type, encoding, charset, language, description, and translator), and data.stringVariants hosts the currently set strings (variant=>value). The main command that this should lead to is: cosesStringCommit (cosesEditorStringID, cosesEditorStringVariantNName, cosesEditorStringVariantNType and cosesEditorStringVariantNValue where N is a number from 0 to as high as required)',
            'cosesEditor.export' => 'The COSES variant export feature. data.variant holds the id of the variant, and data.output holds the XML representation of the variant.',
            );
}

# dataSource.strings.default
sub getDefaultString {
    my $self = shift;
    my($app, $protocol, $string) = @_;
    if ($protocol eq 'stdout') {
        if ($string eq 'cosesEditor.index') {
            return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses">COSES Editor<br/></text>');
        } elsif ($string eq 'cosesEditor.variant') {
            return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses">COSES Editor<br/>Variant Editor<br/></text>');
        } elsif ($string eq 'cosesEditor.string') {
            return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses">COSES Editor<br/>String Editor<br/></text>');
        } elsif ($string eq 'cosesEditor.export') {
            return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses"><text variable="(data.output)"/><br/></text>');
        }
    } elsif ($protocol eq 'http') {
        if ($string eq 'cosesEditor.index') {
            return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses">HTTP/1.1 200 OK<br/>Content-Type: text/plain<br/><br/>COSES Editor: Index</text>');
        } elsif ($string eq 'cosesEditor.variant') {
            return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses">HTTP/1.1 200 OK<br/>Content-Type: text/plain<br/><br/>COSES Editor: Variant Editor</text>');
        } elsif ($string eq 'cosesEditor.string') {
            return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses">HTTP/1.1 200 OK<br/>Content-Type: text/plain<br/><br/>COSES Editor: String Editor</text>');
        } elsif ($string eq 'cosesEditor.export') {
            return ('COSES', '<text xmlns="http://bugzilla.mozilla.org/coses">HTTP/1.1 200 OK<br/>Content-Type: text/xml<br/><br/><text variable="(data.output)"/></text>');
        }
    }
    return; # nope, sorry
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
    my $id = $input->getArgument('cosesEditorVariantID');
    my @data = ();
    foreach my $argument (qw(name protocol quality type encoding charset language description translator)) {
        push(@data, $input->getArgument('cosesEditorVariant\u$argument'));
    }
    my %variantStrings = ();
    my $index = 0;
    while (defined(my $name = $input->getArgument('cosesEditorVariantString${index}Name'))) {
        $variantStrings{$name} = [$input->getArgument('cosesEditorVariantString${index}Type'),
                                  $input->getArgument('cosesEditorVariantString${index}Value')];
        $index += 1;
    }
    my $newName = $input->getArgument('cosesEditorVariantStringNewName');
    if ((defined($newName)) and ($newName ne '')) {
        $variantStrings{$newName} = [$input->getArgument('cosesEditorVariantStringNewType'),
                                     $input->getArgument('cosesEditorVariantStringNewValue')];
    }
    return ($id, \@data, \%variantStrings);
}
