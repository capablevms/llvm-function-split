#!/bin/bash

export LUA=../../lua.shared

rc="0"

for folder in */
do
	if [ -f "$folder/out" ]
	then
		cd $folder
		make -f Makefile test;
		if difference=$(diff out out.diff); then
			echo "success $folder";
		else
			echo "error $folder";
			echo $difference;
			rc="1"
		fi
		rm out.diff
		cd ..;
	else
		cd $folder;
		make -f Makefile run;
		cd ..;
	fi
done

exit $rc
