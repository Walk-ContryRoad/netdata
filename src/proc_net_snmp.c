#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "config.h"
#include "procfile.h"
#include "rrd.h"
#include "plugin_proc.h"

#define RRD_TYPE_NET_SNMP			"ipv4"
#define RRD_TYPE_NET_SNMP_LEN		strlen(RRD_TYPE_NET_SNMP)

int do_proc_net_snmp(int update_every, unsigned long long dt) {
	static procfile *ff = NULL;
	static int do_ip_packets = -1, do_ip_fragsout = -1, do_ip_fragsin = -1, do_ip_errors = -1,
		do_tcp_sockets = -1, do_tcp_packets = -1, do_tcp_errors = -1, do_tcp_handshake = -1, 
		do_udp_packets = -1, do_udp_errors = -1;

	if(do_ip_packets == -1)		do_ip_packets 		= config_get_boolean("plugin:proc:/proc/net/snmp", "ipv4 packets", 1);
	if(do_ip_fragsout == -1)	do_ip_fragsout 		= config_get_boolean("plugin:proc:/proc/net/snmp", "ipv4 fragrments sent", 1);
	if(do_ip_fragsin == -1)		do_ip_fragsin 		= config_get_boolean("plugin:proc:/proc/net/snmp", "ipv4 fragments assembly", 1);
	if(do_ip_errors == -1)		do_ip_errors 		= config_get_boolean("plugin:proc:/proc/net/snmp", "ipv4 errors", 1);
	if(do_tcp_sockets == -1)	do_tcp_sockets 		= config_get_boolean("plugin:proc:/proc/net/snmp", "ipv4 TCP connections", 1);
	if(do_tcp_packets == -1)	do_tcp_packets 		= config_get_boolean("plugin:proc:/proc/net/snmp", "ipv4 TCP packets", 1);
	if(do_tcp_errors == -1)		do_tcp_errors 		= config_get_boolean("plugin:proc:/proc/net/snmp", "ipv4 TCP errors", 1);
	if(do_tcp_handshake == -1)	do_tcp_handshake 	= config_get_boolean("plugin:proc:/proc/net/snmp", "ipv4 TCP handshake issues", 1);
	if(do_udp_packets == -1)	do_udp_packets 		= config_get_boolean("plugin:proc:/proc/net/snmp", "ipv4 UDP packets", 1);
	if(do_udp_errors == -1)		do_udp_errors 		= config_get_boolean("plugin:proc:/proc/net/snmp", "ipv4 UDP errors", 1);

	if(dt) {};

	if(!ff) ff = procfile_open("/proc/net/snmp", " \t:");
	if(!ff) return 1;

	ff = procfile_readall(ff);
	if(!ff) return 0; // we return 0, so that we will retry to open it next time

	uint32_t lines = procfile_lines(ff), l;
	uint32_t words;

	RRD_STATS *st;

	for(l = 0; l < lines ;l++) {
		if(strcmp(procfile_lineword(ff, l, 0), "Ip") == 0) {
			l++;

			if(strcmp(procfile_lineword(ff, l, 0), "Ip") != 0) {
				error("Cannot read Ip line from /proc/net/snmp.");
				break;
			}

			words = procfile_linewords(ff, l);
			if(words < 20) {
				error("Cannot read /proc/net/snmp Ip line. Expected 20 params, read %d.", words);
				continue;
			}

			// see also http://net-snmp.sourceforge.net/docs/mibs/ip.html
			unsigned long long Forwarding, DefaultTTL, InReceives, InHdrErrors, InAddrErrors, ForwDatagrams, InUnknownProtos, InDiscards, InDelivers,
				OutRequests, OutDiscards, OutNoRoutes, ReasmTimeout, ReasmReqds, ReasmOKs, ReasmFails, FragOKs, FragFails, FragCreates;

			Forwarding		= strtoull(procfile_lineword(ff, l, 1), NULL, 10);
			DefaultTTL		= strtoull(procfile_lineword(ff, l, 2), NULL, 10);
			InReceives		= strtoull(procfile_lineword(ff, l, 3), NULL, 10);
			InHdrErrors		= strtoull(procfile_lineword(ff, l, 4), NULL, 10);
			InAddrErrors	= strtoull(procfile_lineword(ff, l, 5), NULL, 10);
			ForwDatagrams	= strtoull(procfile_lineword(ff, l, 6), NULL, 10);
			InUnknownProtos	= strtoull(procfile_lineword(ff, l, 7), NULL, 10);
			InDiscards		= strtoull(procfile_lineword(ff, l, 8), NULL, 10);
			InDelivers		= strtoull(procfile_lineword(ff, l, 9), NULL, 10);
			OutRequests		= strtoull(procfile_lineword(ff, l, 10), NULL, 10);
			OutDiscards		= strtoull(procfile_lineword(ff, l, 11), NULL, 10);
			OutNoRoutes		= strtoull(procfile_lineword(ff, l, 12), NULL, 10);
			ReasmTimeout	= strtoull(procfile_lineword(ff, l, 13), NULL, 10);
			ReasmReqds		= strtoull(procfile_lineword(ff, l, 14), NULL, 10);
			ReasmOKs		= strtoull(procfile_lineword(ff, l, 15), NULL, 10);
			ReasmFails		= strtoull(procfile_lineword(ff, l, 16), NULL, 10);
			FragOKs			= strtoull(procfile_lineword(ff, l, 17), NULL, 10);
			FragFails		= strtoull(procfile_lineword(ff, l, 18), NULL, 10);
			FragCreates		= strtoull(procfile_lineword(ff, l, 19), NULL, 10);
			
			// these are not counters
			if(Forwarding) {};		// is forwarding enabled?
			if(DefaultTTL) {};		// the default ttl on packets
			if(ReasmTimeout) {};	// reassemply timeout

			// this counter is not used
			if(InDelivers) {};		// total number of packets delivered to IP user-protcols

			// --------------------------------------------------------------------

			if(do_ip_packets) {
				st = rrd_stats_find(RRD_TYPE_NET_SNMP ".packets");
				if(!st) {
					st = rrd_stats_create(RRD_TYPE_NET_SNMP, "packets", NULL, RRD_TYPE_NET_SNMP, "IPv4 Packets", "packets/s", 3000, update_every, CHART_TYPE_LINE);

					rrd_stats_dimension_add(st, "received", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "sent", NULL, -1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "forwarded", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
				}
				else rrd_stats_next(st);

				rrd_stats_dimension_set(st, "sent", OutRequests);
				rrd_stats_dimension_set(st, "received", InReceives);
				rrd_stats_dimension_set(st, "forwarded", ForwDatagrams);
				rrd_stats_done(st);
			}

			// --------------------------------------------------------------------

			if(do_ip_fragsout) {
				st = rrd_stats_find(RRD_TYPE_NET_SNMP ".fragsout");
				if(!st) {
					st = rrd_stats_create(RRD_TYPE_NET_SNMP, "fragsout", NULL, RRD_TYPE_NET_SNMP, "IPv4 Fragments Sent", "packets/s", 3010, update_every, CHART_TYPE_LINE);
					st->isdetail = 1;

					rrd_stats_dimension_add(st, "ok", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "failed", NULL, -1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "all", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
				}
				else rrd_stats_next(st);

				rrd_stats_dimension_set(st, "ok", FragOKs);
				rrd_stats_dimension_set(st, "failed", FragFails);
				rrd_stats_dimension_set(st, "all", FragCreates);
				rrd_stats_done(st);
			}

			// --------------------------------------------------------------------

			if(do_ip_fragsin) {
				st = rrd_stats_find(RRD_TYPE_NET_SNMP ".fragsin");
				if(!st) {
					st = rrd_stats_create(RRD_TYPE_NET_SNMP, "fragsin", NULL, RRD_TYPE_NET_SNMP, "IPv4 Fragments Reassembly", "packets/s", 3011, update_every, CHART_TYPE_LINE);
					st->isdetail = 1;

					rrd_stats_dimension_add(st, "ok", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "failed", NULL, -1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "all", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
				}
				else rrd_stats_next(st);

				rrd_stats_dimension_set(st, "ok", ReasmOKs);
				rrd_stats_dimension_set(st, "failed", ReasmFails);
				rrd_stats_dimension_set(st, "all", ReasmReqds);
				rrd_stats_done(st);
			}

			// --------------------------------------------------------------------

			if(do_ip_errors) {
				st = rrd_stats_find(RRD_TYPE_NET_SNMP ".errors");
				if(!st) {
					st = rrd_stats_create(RRD_TYPE_NET_SNMP, "errors", NULL, RRD_TYPE_NET_SNMP, "IPv4 Errors", "packets/s", 3002, update_every, CHART_TYPE_LINE);
					st->isdetail = 1;

					rrd_stats_dimension_add(st, "InDiscards", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "OutDiscards", NULL, -1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);

					rrd_stats_dimension_add(st, "InHdrErrors", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "InAddrErrors", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "InUnknownProtos", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);

					rrd_stats_dimension_add(st, "OutNoRoutes", NULL, -1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
				}
				else rrd_stats_next(st);

				rrd_stats_dimension_set(st, "InDiscards", InDiscards);
				rrd_stats_dimension_set(st, "OutDiscards", OutDiscards);
				rrd_stats_dimension_set(st, "InHdrErrors", InHdrErrors);
				rrd_stats_dimension_set(st, "InAddrErrors", InAddrErrors);
				rrd_stats_dimension_set(st, "InUnknownProtos", InUnknownProtos);
				rrd_stats_dimension_set(st, "OutNoRoutes", OutNoRoutes);
				rrd_stats_done(st);
			}
		}
		else if(strcmp(procfile_lineword(ff, l, 0), "Tcp") == 0) {
			l++;

			if(strcmp(procfile_lineword(ff, l, 0), "Tcp") != 0) {
				error("Cannot read Tcp line from /proc/net/snmp.");
				break;
			}

			words = procfile_linewords(ff, l);
			if(words < 15) {
				error("Cannot read /proc/net/snmp Tcp line. Expected 15 params, read %d.", words);
				continue;
			}

			unsigned long long RtoAlgorithm, RtoMin, RtoMax, MaxConn, ActiveOpens, PassiveOpens, AttemptFails, EstabResets,
				CurrEstab, InSegs, OutSegs, RetransSegs, InErrs, OutRsts;

			RtoAlgorithm	= strtoull(procfile_lineword(ff, l, 1), NULL, 10);
			RtoMin			= strtoull(procfile_lineword(ff, l, 2), NULL, 10);
			RtoMax			= strtoull(procfile_lineword(ff, l, 3), NULL, 10);
			MaxConn			= strtoull(procfile_lineword(ff, l, 4), NULL, 10);
			ActiveOpens		= strtoull(procfile_lineword(ff, l, 5), NULL, 10);
			PassiveOpens	= strtoull(procfile_lineword(ff, l, 6), NULL, 10);
			AttemptFails	= strtoull(procfile_lineword(ff, l, 7), NULL, 10);
			EstabResets		= strtoull(procfile_lineword(ff, l, 8), NULL, 10);
			CurrEstab		= strtoull(procfile_lineword(ff, l, 9), NULL, 10);
			InSegs			= strtoull(procfile_lineword(ff, l, 10), NULL, 10);
			OutSegs			= strtoull(procfile_lineword(ff, l, 11), NULL, 10);
			RetransSegs		= strtoull(procfile_lineword(ff, l, 12), NULL, 10);
			InErrs			= strtoull(procfile_lineword(ff, l, 13), NULL, 10);
			OutRsts			= strtoull(procfile_lineword(ff, l, 14), NULL, 10);

			// these are not counters
			if(RtoAlgorithm) {};
			if(RtoMin) {};
			if(RtoMax) {};
			if(MaxConn) {};

			// --------------------------------------------------------------------
			
			// see http://net-snmp.sourceforge.net/docs/mibs/tcp.html
			if(do_tcp_sockets) {
				st = rrd_stats_find(RRD_TYPE_NET_SNMP ".tcpsock");
				if(!st) {
					st = rrd_stats_create(RRD_TYPE_NET_SNMP, "tcpsock", NULL, "tcp", "IPv4 TCP Connections", "active connections", 2500, update_every, CHART_TYPE_LINE);

					rrd_stats_dimension_add(st, "connections", NULL, 1, 1, RRD_DIMENSION_ABSOLUTE);
				}
				else rrd_stats_next(st);

				rrd_stats_dimension_set(st, "connections", CurrEstab);
				rrd_stats_done(st);
			}

			// --------------------------------------------------------------------
			
			if(do_tcp_packets) {
				st = rrd_stats_find(RRD_TYPE_NET_SNMP ".tcppackets");
				if(!st) {
					st = rrd_stats_create(RRD_TYPE_NET_SNMP, "tcppackets", NULL, "tcp", "IPv4 TCP Packets", "packets/s", 2600, update_every, CHART_TYPE_LINE);

					rrd_stats_dimension_add(st, "received", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "sent", NULL, -1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
				}
				else rrd_stats_next(st);

				rrd_stats_dimension_set(st, "received", InSegs);
				rrd_stats_dimension_set(st, "sent", OutSegs);
				rrd_stats_done(st);
			}

			// --------------------------------------------------------------------
			
			if(do_tcp_errors) {
				st = rrd_stats_find(RRD_TYPE_NET_SNMP ".tcperrors");
				if(!st) {
					st = rrd_stats_create(RRD_TYPE_NET_SNMP, "tcperrors", NULL, "tcp", "IPv4 TCP Errors", "packets/s", 2700, update_every, CHART_TYPE_LINE);
					st->isdetail = 1;

					rrd_stats_dimension_add(st, "InErrs", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "RetransSegs", NULL, -1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
				}
				else rrd_stats_next(st);

				rrd_stats_dimension_set(st, "InErrs", InErrs);
				rrd_stats_dimension_set(st, "RetransSegs", RetransSegs);
				rrd_stats_done(st);
			}

			// --------------------------------------------------------------------
			
			if(do_tcp_handshake) {
				st = rrd_stats_find(RRD_TYPE_NET_SNMP ".tcphandshake");
				if(!st) {
					st = rrd_stats_create(RRD_TYPE_NET_SNMP, "tcphandshake", NULL, "tcp", "IPv4 TCP Handshake Issues", "events/s", 2900, update_every, CHART_TYPE_LINE);
					st->isdetail = 1;

					rrd_stats_dimension_add(st, "EstabResets", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "OutRsts", NULL, -1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "ActiveOpens", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "PassiveOpens", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "AttemptFails", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
				}
				else rrd_stats_next(st);

				rrd_stats_dimension_set(st, "EstabResets", EstabResets);
				rrd_stats_dimension_set(st, "OutRsts", OutRsts);
				rrd_stats_dimension_set(st, "ActiveOpens", ActiveOpens);
				rrd_stats_dimension_set(st, "PassiveOpens", PassiveOpens);
				rrd_stats_dimension_set(st, "AttemptFails", AttemptFails);
				rrd_stats_done(st);
			}
		}
		else if(strcmp(procfile_lineword(ff, l, 0), "Udp") == 0) {
			l++;

			if(strcmp(procfile_lineword(ff, l, 0), "Udp") != 0) {
				error("Cannot read Udp line from /proc/net/snmp.");
				break;
			}

			words = procfile_linewords(ff, l);
			if(words < 7) {
				error("Cannot read /proc/net/snmp Udp line. Expected 7 params, read %d.", words);
				continue;
			}

			unsigned long long InDatagrams, NoPorts, InErrors, OutDatagrams, RcvbufErrors, SndbufErrors;

			InDatagrams		= strtoull(procfile_lineword(ff, l, 1), NULL, 10);
			NoPorts			= strtoull(procfile_lineword(ff, l, 2), NULL, 10);
			InErrors		= strtoull(procfile_lineword(ff, l, 3), NULL, 10);
			OutDatagrams	= strtoull(procfile_lineword(ff, l, 4), NULL, 10);
			RcvbufErrors	= strtoull(procfile_lineword(ff, l, 5), NULL, 10);
			SndbufErrors	= strtoull(procfile_lineword(ff, l, 6), NULL, 10);

			// --------------------------------------------------------------------
			
			// see http://net-snmp.sourceforge.net/docs/mibs/udp.html
			if(do_udp_packets) {
				st = rrd_stats_find(RRD_TYPE_NET_SNMP ".udppackets");
				if(!st) {
					st = rrd_stats_create(RRD_TYPE_NET_SNMP, "udppackets", NULL, "udp", "IPv4 UDP Packets", "packets/s", 2601, update_every, CHART_TYPE_LINE);

					rrd_stats_dimension_add(st, "received", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "sent", NULL, -1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
				}
				else rrd_stats_next(st);

				rrd_stats_dimension_set(st, "received", InDatagrams);
				rrd_stats_dimension_set(st, "sent", OutDatagrams);
				rrd_stats_done(st);
			}

			// --------------------------------------------------------------------
			
			if(do_udp_errors) {
				st = rrd_stats_find(RRD_TYPE_NET_SNMP ".udperrors");
				if(!st) {
					st = rrd_stats_create(RRD_TYPE_NET_SNMP, "udperrors", NULL, "udp", "IPv4 UDP Errors", "events/s", 2701, update_every, CHART_TYPE_LINE);
					st->isdetail = 1;

					rrd_stats_dimension_add(st, "RcvbufErrors", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "SndbufErrors", NULL, -1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "InErrors", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
					rrd_stats_dimension_add(st, "NoPorts", NULL, 1, 1 * update_every, RRD_DIMENSION_INCREMENTAL);
				}
				else rrd_stats_next(st);

				rrd_stats_dimension_set(st, "InErrors", InErrors);
				rrd_stats_dimension_set(st, "NoPorts", NoPorts);
				rrd_stats_dimension_set(st, "RcvbufErrors", RcvbufErrors);
				rrd_stats_dimension_set(st, "SndbufErrors", SndbufErrors);
				rrd_stats_done(st);
			}
		}
	}
	
	return 0;
}
