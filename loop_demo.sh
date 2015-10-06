#!/bin/bash

export DEMO_FILE=/repono/ETALON_PICTURES/BRA/ITV/Video/Canon5DMkIII_3/3lines_test.avi

for (( ; ; ))
do
	./vpwfetch-oa2410 -S [vpw,vpwi-br] -y 1 -k 5 -X [2,2] --if $DEMO_FILE
done