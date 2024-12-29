#!/usr/bin/env bash

./benchmark --cache_th 0.15 --no_thread 2 --get_ratio 50 --ip_master 192.168.100.12 --ip_worker 192.168.100.21 --port_worker 2216 --is_master 1 --port_master 2214 > ../log/kv_log