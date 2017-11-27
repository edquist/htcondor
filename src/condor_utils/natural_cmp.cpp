#include <ctype.h>
#include <string>
#include <algorithm>

// "natural" string compare -- a replacement for strcmp(3)
// takes numeric portions into account, as specified in strverscmp(3)
int natural_cmp(const std::string &str1, const std::string &str2)
{
	typedef std::string::const_iterator iterator;
	iterator s1 = str1.begin();
	iterator s2 = str2.begin();
	iterator s1e = str1.end();
	iterator s2e = str2.end();

	iterator s1_beg = s1;     // s1 begin
	iterator n1_beg, n2_beg;  // s1/s2 number begin
	iterator n1_end, n2_end;  // s1/s2 number end
	iterator z1_end, z2_end;  // s1/s2 zeros end

	// find first mismatch
	std::pair<iterator,iterator> mm = std::mismatch(s1, s1e, s2);
	s1 = mm.first;
	s2 = mm.second;
	if (s1 == s1e && s2 == s2e) {
		return 0;
	}

	// find digits leading up to mismatch
	for (n1_beg = s1; n1_beg > s1_beg && isdigit(n1_beg[-1]); --n1_beg) {}
	n2_beg = s2 - (s1 - n1_beg);

	// just compare mismatch unless it touches a digit in both strings
	if (n1_beg == s1 && (!isdigit(*s1) || !isdigit(*s2))) {
		return *s1 - *s2;
	}

	// find leading zeros
	for (z1_end = n1_beg; *z1_end == '0'; ++z1_end) {}
	for (z2_end = n2_beg; *z2_end == '0'; ++z2_end) {}

	// don't count a final trailing zero as a leading zero
	if (z1_end > n1_beg && !isdigit(*z1_end)) {
		--z1_end;
	}
	if (z2_end > n2_beg && !isdigit(*z2_end)) {
		--z2_end;
	}

	// fewer leading zeros comes first
	if (z1_end - n1_beg != z2_end - n2_beg) {
		return (z2_end - n2_beg) - (z1_end - n1_beg);
	}

	// for an equal positive number of leading zeros, compare mismatch
	if (z1_end > n1_beg) {
		return *s1 - *s2;
	}

	// no leading zeros, the rest is an arbitrary length numeric compare
	for (n1_end = z1_end; isdigit(*n1_end); ++n1_end) {}
	for (n2_end = z2_end; isdigit(*n2_end); ++n2_end) {}

	if (n1_end - n1_beg != n2_end - n2_beg) {
		return (n1_end - n1_beg) - (n2_end - n2_beg);
	} else {
		return *s1 - *s2;
	}
}

