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

package PLIF::Service::Component::CosesEditor;
use strict;
use vars qw(@ISA);
use PLIF::Service;
@ISA = qw(PLIF::Service);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'component.cosesEditor' or
            $service eq 'dispatcher.commands' or 
            $service eq 'dispatcher.output.generic' or 
            $service eq 'dataSource.strings.default' or
            $class->SUPER::provides($service));
}

# dispatcher.commands
sub cmdCosesEditor {
    # warning: this is also called from other methods below
    my $self = shift;
    my($app) = @_;
    my %variants = $app->getService('dataSource.strings')->getDescribedVariants();
    my $variantsSortColumn = $app->input->getArgument('cosesEditorVariantsSortColumn');
    my %strings = @{$app->getCollectingServiceList('dispatcher.output')->strings};
    my $stringsSortColumn = $app->input->getArgument('cosesEditorStringsSortColumn');
    my $user = $app->getObject('user');
    if (defined($user)) {
        # XXX have to register these settings
        $user->setting(\$variantsSortColumn, 'cosesEditor.index.variantsSortColumn');
        $user->setting(\$stringsSortColumn, 'cosesEditor.index.stringsSortColumn');
    }
    $app->output->cosesEditorIndex(\%variants, $variantsSortColumn, \%strings, $stringsSortColumn);
}

# dispatcher.commands
sub cmdCosesVariantAdd {
    my $self = shift;
    my($app) = @_;
    my @data = ('', '', 1.0, '', '', '', '', '', '');
    my $id = $app->getService('dataSource.strings')->setVariant($app, undef, @data);
    my %expectedStrings = @{$app->getCollectingServiceList('dispatcher.output')->strings};
    $app->output->cosesEditorVariant($id, @data, \%expectedStrings, {});
}

# dispatcher.commands
sub cmdCosesVariantEdit {
    my $self = shift;
    my($app) = @_;
    my $id = $app->input->getArgument('cosesEditorVariantID');
    my $dataSource = $app->getService('dataSource.strings');
    my @data = $dataSource->getVariant($app, $id);
    my %expectedStrings = @{$app->getCollectingServiceList('dispatcher.output')->strings};
    my %variantStrings = $dataSource->getVariantStrings($app, $id);
    $app->output->cosesEditorVariant($id, @data, \%expectedStrings, \%variantStrings);
}

# dispatcher.commands
sub cmdCosesVariantAddString {
    my $self = shift;
    my($app) = @_;
    my %expectedStrings = @{$app->getCollectingServiceList('dispatcher.output')->strings};
    my($id, $data, $variantStrings) = $self->getVariantEditorArguments($app);
    $app->output->cosesEditorVariant($id, @$data, \%expectedStrings, $variantStrings);
}

# dispatcher.commands
sub cmdCosesVariantCommit {
    my $self = shift;
    my($app) = @_;
    my($id, $data, $variantStrings) = $self->getVariantEditorArguments($app);
    my $dataSource = $app->getService('dataSource.strings');
    $dataSource->setVariant($app, $id, @$data);
    foreach my $string (keys(%$variantStrings)) {
        $dataSource->setString($app, $id, $string, $variantStrings->{$string});
    }
    $self->cmdCosesEditor($app);
}

# dispatcher.commands
sub cmdCosesStringEdit {
    my $self = shift;
    my($app) = @_;
    my $id = $app->input->getArgument('cosesEditorStringID');
    my %strings = @{$app->getCollectingServiceList('dispatcher.output')->strings};
    my @variants = $app->getService('dataSource.strings')->getStringVariants($app, $id);
    $app->output->cosesEditorString($id, $strings{$id}, \@variants);
}

# dispatcher.commands
sub cmdCosesStringCommit {
    my $self = shift;
    my($app) = @_;
    my $input = $app->input;
    my $id = $input->getArgument('cosesEditorStringID');
    my %variants = ();
    my $index = 0;
    while (defined(my $name = $input->getArgument('cosesEditorStringVariant${index}Name'))) {
        $variants{$name} = $input->getArgument('cosesEditorStringVariant${index}Value');
        $index += 1;
    }
    my $dataSource = $app->getService('dataSource.strings');
    foreach my $variant (keys(%variants)) {
        $dataSource->setString($app, $variant, $id, $variants{$variant});
    }
    $self->cmdCosesEditor($app);
}

# dispatcher.commands
sub cmdCosesVariantExport {
    my $self = shift;
    my($app) = @_;

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
        my $value = $XML->escape($strings{$string});
        $result.= "  <string name=\"$name\">$value</string>\n";
    }
    $result .= '</variant>';
    $app->output->cosesExport($id, $result);
}

# dispatcher.commands
sub cmdCosesVariantImport {
    my $self = shift;
    my($app) = @_;

    # get data
    my $file = $app->input->getArgument('cosesEditorImportData');

    # parse data
    my $XML = $app->getService('service.xml');
    my $data = {
        'depth' => 0,
        'string' => undef,
        'variant' => [], # same at all scopes (because walkNesting() is not a deep copy)
        'strings' => {}, # same at all scopes (because walkNesting() is not a deep copy)
    };
    $XML->walk($self, $XML->parse($file), $data);

    # use data
    my $dataSource = $app->getService('dataSource.strings');
    my $id = $dataSource->setVariant($app, undef, @{$data->{'variant'}});
    foreach my $string (keys(%{$data->{'strings'}})) {
        $dataSource->setString($app, $id, $string, $data->{'strings'}->{$string});
    }
    
    # display data                    
    my %expectedStrings = @{$app->getCollectingServiceList('dispatcher.output')->strings};
    $app->output->cosesEditorVariant($id, @{$data->{'variant'}}, \%expectedStrings, $data->{'strings'}->{$string});
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
            } else {
                $self->error(1, 'invalid variant document format - missing attribute 'name' on <string>');
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
        $data->{'strings'}->{$data->{'string'}} = $text;
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


# XXXXXX

# dispatcher.output.generic
sub outputCosesEditor {
    my $self = shift;
    my($app, $output, XXX) = @_;
    $output->output('cosesEditor', {
    });   
}

# dispatcher.output.generic
# XXX

# dataSource.strings.default
sub getDefaultString {
    my $self = shift;
    my($app, $protocol, $string) = @_;
    # XXX
    if ($protocol eq 'stdout') {
        if ($string eq 'cosesEditor') {
            return '<text>COSES Editor<br/></text>';
        }
    } elsif ($protocol eq 'http') {
        if ($string eq 'cosesEditor') {
            return '<text>HTTP/1.1 200 OK<br/>Content-Type: text/plain<br/><br/>COSES Editor</text>';
        }
    }
    return; # nope, sorry
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
        $variantStrings{$name} = $input->getArgument('cosesEditorVariantString${index}Value');
        $index += 1;
    }
    my $newName = $input->getArgument('cosesEditorVariantStringNewName');
    if ((defined($newName)) and ($newName ne '')) {
        $variantStrings{$newName} = $input->getArgument('cosesEditorVariantStringNewValue');    
    }
    return ($id, \@data, \%variantStrings);
}
