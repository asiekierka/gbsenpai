#!/usr/bin/python3

import sys

with open(sys.argv[-1]) as f:
	fs = f.read()
	for i in range(0, 32):
		if f"bank_{i}_data[]" in fs:
			print(f"if (id == {i}) return bank_{i}_data;")
