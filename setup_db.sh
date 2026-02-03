#!/bin/bash
echo "lenovo112423" | sudo -S -u postgres psql -c "CREATE USER bingosys WITH PASSWORD 'bingosys';"
echo "lenovo112423" | sudo -S -u postgres psql -c "CREATE DATABASE bingosys OWNER bingosys;"
