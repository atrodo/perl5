package CLASS;
use v5.12;

our $VERSION = '0.01';

sub import {
    return unless $_[0] eq __PACKAGE__;
    return unless @_ > 1;
    require Carp;
    Carp::croak("CLASS does not export anything");
}

state %has_sort;

sub _DEFINE_FIELD
{
  my ($pkg, $name, $rlexname, $init, @attrs) = @_;

  my $pkgHAS = eval "\\%${pkg}::HAS";
  require Data::Dumper;
  warn Data::Dumper::Dumper(@_, $pkgHAS );

  my %has = (
    field => $name, member => $name,
    read => 1, write => 0,
    sort => scalar( keys %$pkgHAS ),
  );

  if ( defined $init )
  {
    die "field assignment to a reference can only be to code"
      if ref $init && ref $init ne 'CODE';
    $has{init} = $init;
  }

  my ($call, $filename, $line) = caller;
  $pkg->MODIFY_FIELD_ATTRIBUTE($pkg, \%has, @attrs);
  eval "# line $line $filename\nuse strict; use feature q{class}; class $pkg { member \$$has{member}; sub _x_borked {}; method $has{field} { \$$has{member} } }";
  die $@ if $@;
  $$rlexname = "$has{member}";

  @{$has_sort{$pkg} ||= []}[$has{sort}] = "$has{field}";

  $pkgHAS->{$has{field}} = \%has;
}

sub MODIFY_FIELD_ATTRIBUTE
{
  my $pkg = shift;
  my $has = shift;
  return @_;
}

sub new {
  my $class = shift;
  $class = CORE::ref($class) ? CORE::ref($class) : $class;
  my $obj = bless({ ref($_[0]) eq ref {} ? %$_[0] : @_ }, $class);

  my $pkgHAS = eval "\\%${class}::HAS";
  foreach my $field ( @{ $has_sort{$class} || [] } )
  {
    warn $field;
    my $has = $pkgHAS->{$field};
    if ( exists $obj->{$field} )
    {
      $obj->{ $has->{member} } = delete $obj->{ $has->{field} };
    }
    elsif ( defined $has->{init} )
    {
      my $init = $has->{init};
      $obj->{ $has->{member} } = ref $init ? $init->($obj) : $init;
    }
  }

  return $obj;
}

1;
