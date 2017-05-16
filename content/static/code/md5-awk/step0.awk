BEGIN {
	_ord_init();
}

{
	# NOTE: remember the input files in-order in the `files' array.
	if (nfiles == 0 || files[nfiles] != FILENAME)
		files[++nfiles] = FILENAME;
	# XXX: only work with files ending with a newline, this is an OK
	# limitation since it is required by POSIX.
	content[FILENAME] = content[FILENAME] $0 "\n";
}

END {
	# go over all the files in-order.
	for (i = 1; i <= nfiles; i++) {
		fn = files[i];
		# a-la `openssl md5' output.
		printf("MD5(%s)= %s\n", fn, md5(content[fn]));
	}
}

# our md5 implementation
function md5(input,    nbytes, chars, i, bytes, hi, lo, words, nwords) {
	# convert the input into an array of bytes using ord() on each
	# character.
	nbytes = split(input, chars, "");
	for (i = 1; i <= nbytes; i++)
		bytes[i] = ord(chars[i]);

	# convert the array of bytes into an array of 32-bits words.
	# NOTE: words is 0-indexed.
	for (i = 1; i <= nbytes; i += 4) {
		hi = or(lshift(bytes[i + 3], 8), bytes[i + 2]);
		lo = or(lshift(bytes[i + 1], 8), bytes[i + 0]);
		words[nwords++] = or(lshift(hi, 16), lo);
	}

	# XXX: debug words and exit:
	for (i = 0; i < nwords; i++)
		printf "%4d: %08x\n", i, words[i];
	exit;
	# NOTREACHED
}

# from https://www.gnu.org/software/gawk/manual/html_node/Ordinal-Functions.html
function _ord_init(    i)
{
	for (i = 0; i < 256; i++)
		_ord_[sprintf("%c", i)] = i;
}

function ord(s)
{
	# only first character is of interest
	return _ord_[substr(s, 1, 1)];
}
