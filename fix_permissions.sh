#!/bin/bash
PASS="lenovo112423"
echo "$PASS" | sudo -S -u postgres psql -d bingosys -c "GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA public TO bingosys;"
echo "$PASS" | sudo -S -u postgres psql -d bingosys -c "GRANT ALL PRIVILEGES ON ALL SEQUENCES IN SCHEMA public TO bingosys;"
