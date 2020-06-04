{ Public function; atoi() is provided by another objects through fungw }
function hello(how_many; who);
begin
	fawk_print('Hello', how_many, who);
	fawk_print('atoi:', atoi('42'));
	hello := 66;
end;

function main(ARG);
begin
	fawk_print('hello world from init', atoi('33'));

	{ register hello(), to be called from other objects and the host application }
	fgw_func_reg(hello);
end;
