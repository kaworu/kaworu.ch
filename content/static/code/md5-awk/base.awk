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
function md5(input) {
	# TODO
}
