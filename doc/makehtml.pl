#!/usr/bin/perl
$printbr = 1;
$printtable = 0;
while (<>) {
	s/\\&//g;
	s/&/&amp;/g;
	s/\</&lt;/g;
	s/\>/&gt;/g;
	s/\\'/'/g;
	s/\\-/-/g;
	if ($printtable == 1) {
		$printtable = 2;
	} elsif ($printtable == 2) {
		$printtable = 3;
		print "</TD><TD ALIGN=\"left\">";
	}

	#hack, we're inside .nf so highlight comments and
	# other such stuff
	if ($printbr == 0) {
		s/\/\*/<FONT COLOR="#5555ff">\/\*/g;
		s/\*\//\*\/<\/FONT>/g;
	}
	
	if (/^\.\\"/) {
		# comment ignore
	} elsif (/^\.TH .*\"([^"]*)\"/) {
		# must be at the front
		print "<HTML><HEAD><TITLE>$1 Manual page</TITLE></HEAD>\n";
		print "<BODY BGCOLOR=\"#ffffff\">\n";
		print "<H1>$1 Manual page</H1>\n";
	} elsif (/^\.SH (.*)$/) {
		if ($printtable == 3) {
			$printtable = 0;
			print "</TD></TR></TABLE>\n";
		}
		print "<H3>$1</H3>\n";
	} elsif (/^\.PP/) {
		if ($printtable == 3) {
			$printtable = 0;
			print "</TD></TR></TABLE>\n";
		}
		print "<P>\n";
	} elsif (/^\.IP/) {
		if ($printtable == 3) {
			$printtable = 0;
			print "</TD></TR></TABLE>\n";
		}
		print "<P>\n";
	} elsif (/^\.B "(.*)"$/) {
		print "<B>$1</B>\n";
	} elsif (/^\.B (.*)$/) {
		print "<B>$1</B>\n";
	} elsif (/^\.I "(.*)"$/) {
		print "<I>$1</I>\n";
	} elsif (/^\.I (.*)$/) {
		print "<I>$1</I>\n";
	} elsif (/^\.nf/) {
		if ($printtable == 3) {
			$printtable = 0;
			print "</TD></TR></TABLE>\n";
		}
		print "<TABLE WIDTH=100%><TR><TD BGCOLOR=\"#dddddd\"><PRE>\n";
		$printbr = 0;
	} elsif (/^\.fi/) {
		print "</PRE>\n";
		print "</TD></TR></TABLE>\n";
		$printbr = 1;
	} elsif (/^[ 	]*$/) {
		if ($printtable == 3) {
			$printtable = 0;
			print "</TD></TR></TABLE>\n";
		}
		if ($printbr) {
			print "<BR>\n";
		} else {
			print "\n";
		}
	} elsif (/^\.TP/) {
		if ($printtable == 3) {
			$printtable = 1;
			print "</TD></TR><TR>\n<TD ALIGN=\"left\">";
		} else {
			$printtable = 1;
			print "<TABLE BORDER><TR>\n<TD ALIGN=\"left\">";
		}
	} else {
		s/\\fB([^\\]*)\\fR/<B>\1<\/B>/gm;
		s/\\fI([^\\]*)\\fR/<I>\1<\/I>/gm;
		s/&lt;([a-zA-Z0-9.]+\@[a-zA-Z0-9.]+)&gt;/&lt;<a href=\"mailto:\1\">\1<\/a>&gt;/g;
		s/(http:\/\/[a-zA-Z0-9.\/~]+\.html)/<a href=\"\1\">\1<\/a>/g;
		print;
	}
}

print "</BODY></HTML>\n";
