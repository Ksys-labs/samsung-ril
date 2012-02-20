#!/bin/sh
#
# Copyright 2012 Paul Kocialkowski, GPLv3+
#
# This script is a dirty hack, keep in mind that is was written in a hurry
# and doesn't reflect our code cleanness standards. 
# Any (working) replacement written in a cleaner way, such as a perl script
# would be greatly appreciated.

echo "/**"
echo " * This list was generated from http://en.wikipedia.org/wiki/Mobile_Network_Code"
echo " * "
echo " * Date: "$( date "+%x %X" )
echo " * Copyright: Wikipedia Contributors, Creative Commons Attribution-ShareAlike License"
echo " */"
echo ""
echo "#ifndef _PLMN_LIST_H_"
echo "#define _PLMN_LIST_H_"
echo ""
echo "struct plmn_list_entry {"
echo "	unsigned int mcc;"
echo "	unsigned int mnc;"
echo "	char *operator_long;"
echo "	char *operator_short;"
echo "};"
echo ""
echo "struct plmn_list_entry plmn_list[] = {"

wget "http://en.wikipedia.org/w/index.php?title=Special:Export&pages=Mobile_Network_Code&action=submit" --quiet -O - | tr -d '\n' | sed -e "s|.*<text[^>]*>\(.*\)</text>.*|\1|g" -e "s/|-/\n|-\n/g" | sed -e "s/\(}===.*\)/\n\1/g" -e "s/===={.*/===={\n/g" -e "s/\&amp;/\&/g" -e "s/\&lt;[^\&]*\&gt;//g" -e "s/&quot;//g" -e "s#\[http[^]]*\]##g" -e "s#\[\[\([^]|]*\)|\([^]]*\)\]\]#\2#g" -e "s#\[\[\([^]]*\)\]\]#\1#g" -e "s#\[\([^] ]*\) \([^]]*\)\]#\2#g" | tail -n +2 | sed "s|.*=== \(.*\) ===.*|// \1|g" | grep -v "|-" | while read line
do
	if [ "$line" = "" ]
	then
		continue
	fi

	test=$( echo "$line" | grep -P "^//" )

	if [ ! "$test" = "" ]
	then
		echo "\n\t$line\n" | sed -e "s#[^|]*|\(.*\)#// \1#g" -e "s/^ //g" -e "s/ $//g"
		continue
	fi

	test=$( echo "$line" | grep "||" )

	if [ "$test" = "" ]
	then
		continue
	fi

	mcc=$( echo "$line" | sed -e "s#[^|]*|[ ]*\([^|]*\)[ ]*||[ ]*\([^|]*\)[ ]*||[ ]*\([^|]*\)[ ]*||[ ]*\([^|]*\).*#\1#g" -e "s/^ //g" -e "s/ $//g" -e "s/[^1-9]*\([0-9]*\).*/\1/g")
	mnc=$( echo "$line" | sed -e "s#[^|]*|[ ]*\([^|]*\)[ ]*||[ ]*\([^|]*\)[ ]*||[ ]*\([^|]*\)[ ]*||[ ]*\([^|]*\).*#\2#g" -e "s/^ //g" -e "s/ $//g" -e "s/[^1-9]*\([0-9]*\).*/\1/g")
	brand=$( echo "$line" | sed -e "s#[^|]*|[ ]*\([^|]*\)[ ]*||[ ]*\([^|]*\)[ ]*||[ ]*\([^|]*\)[ ]*||[ ]*\([^|]*\).*#\3#g" -e "s/^ //g" -e "s/ $//g" )
	operator=$( echo "$line" | sed -e "s#[^|]*|[ ]*\([^|]*\)[ ]*||[ ]*\([^|]*\)[ ]*||[ ]*\([^|]*\)[ ]*||[ ]*\([^|]*\).*#\4#g" -e "s/^ //g" -e "s/ $//g" )

	if [ "$mcc" = "" ] || [ "$mcc" = "?" ]
	then
		continue
	fi

	if [ "$mnc" = "" ] || [ "$mnc" = "?" ]
	then
		continue
	fi

	if [ "$brand" = "" ]
	then
		if [ "$operator" = "" ]
		then
			continue
		fi

		echo "\t{ $mcc, $mnc, \"$operator\", \"$operator\" },"
	else
		echo "\t{ $mcc, $mnc, \"$brand\", \"$brand\" },"
	fi
done

echo "};"
echo ""
echo "#endif"
