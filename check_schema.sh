#!/bin/bash
PASS="lenovo112423"
echo "$PASS" | sudo -S -u postgres psql -P pager=off -d bingosys -c "\d sub_premios"
