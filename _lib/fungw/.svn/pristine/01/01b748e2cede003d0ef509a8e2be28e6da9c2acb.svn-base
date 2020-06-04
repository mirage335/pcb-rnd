# Public function; atoi() is provided by another objects through fungw
sub hello {
	my($how_many, $who) = @_;
	print("Hello " . $how_many . " " . $who . "\n");
	print("atoi: " . atoi("42") . "\n");
	return 66;
}

print("hello world from init " . atoi("33") . "\n");

# register hello(), to be called from other objects and the host application
fgw_func_reg("hello");
