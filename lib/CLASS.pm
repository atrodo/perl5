package CLASS;

our $VERSION = '0.01';

sub import {
    return unless $_[0] eq __PACKAGE__;
    return unless @_ > 1;
    require Carp;
    Carp::croak("CLASS does not export anything");
}

sub _DEFINE_FIELD
{
  my ($pkg, $name, $rlexname) = @_;
  require Data::Dumper;
  eval "use strict; use feature 'class'; class $pkg { method $name { \$$name } }";
  warn Data::Dumper::Dumper(@_, $@);
  $$rlexname = "_$name";
}

sub MODIFY_FIELD_ATTRIBUTE
{
  return @_[1];
}

1;
