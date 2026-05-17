CC = gcc
CFLAGS = -Wall -O2
TARGETS = system_info network_stats auth dmz monitoring tasks traffic vpn firewall ids quota logger

all: $(TARGETS)

system_info: system_info.c
	$(CC) $(CFLAGS) -o system_info system_info.c

network_stats: network_stats.c
	$(CC) $(CFLAGS) -o network_stats network_stats.c

auth: auth.c
	$(CC) $(CFLAGS) -o auth auth.c

dmz: dmz.c
	$(CC) $(CFLAGS) -o dmz dmz.c

monitoring: monitoring.c
	$(CC) $(CFLAGS) -o monitoring monitoring.c

tasks: tasks.c
	$(CC) $(CFLAGS) -o tasks tasks.c

traffic: traffic.c
	$(CC) $(CFLAGS) -o traffic traffic.c

vpn: vpn.c
	$(CC) $(CFLAGS) -o vpn vpn.c

firewall: firewall.c
	$(CC) $(CFLAGS) -o firewall firewall.c

ids: ids.c
	$(CC) $(CFLAGS) -o ids ids.c

quota: quota.c
	$(CC) $(CFLAGS) -o quota quota.c

logger: logger.c
	$(CC) $(CFLAGS) -o logger logger.c

clean:
	rm -f $(TARGETS)

install: all
	cp $(TARGETS) /var/www/cgi-bin/
	chmod +x /var/www/cgi-bin/$(TARGETS)

.PHONY: all clean install
