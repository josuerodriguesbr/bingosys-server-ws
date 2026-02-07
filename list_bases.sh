#!/bin/bash
PASS="lenovo112423"
echo "$PASS" | sudo -S -u postgres psql -P pager=off -d bingosys -c "SELECT id, nome FROM bases_dados ORDER BY id LIMIT 5;"
