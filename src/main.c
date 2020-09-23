/*
 * iperf, Copyright (c) 2014, 2015, 2017, 2019, The Regents of the University of
 * California, through Lawrence Berkeley National Laboratory (subject
 * to receipt of any required approvals from the U.S. Dept. of
 * Energy).  All rights reserved.
 *
 * If you have questions about your rights to use or distribute this
 * software, please contact Berkeley Lab's Technology Transfer
 * Department at TTD@lbl.gov.
 *
 * NOTICE.  This software is owned by the U.S. Department of Energy.
 * As such, the U.S. Government has been granted for itself and others
 * acting on its behalf a paid-up, nonexclusive, irrevocable,
 * worldwide license in the Software to reproduce, prepare derivative
 * works, and perform publicly and display publicly.  Beginning five
 * (5) years after the date permission to assert copyright is obtained
 * from the U.S. Department of Energy, and subject to any subsequent
 * five (5) year renewals, the U.S. Government is granted for itself
 * and others acting on its behalf a paid-up, nonexclusive,
 * irrevocable, worldwide license in the Software to reproduce,
 * prepare derivative works, distribute copies to the public, perform
 * publicly and display publicly, and to permit others to do so.
 *
 * This code is distributed under a BSD style license, see the LICENSE
 * file for complete information.
 */
#include "iperf_config.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "iperf.h"
#include "iperf_api.h"
#include "units.h"
#include "iperf_locale.h"
#include "net.h"


static int run(struct iperf_test *test);


/**************************************************************************/
int
old_main(int argc, char **argv)
{
    struct iperf_test *test;

    // XXX: Setting the process affinity requires root on most systems.
    //      Is this a feature we really need?
#ifdef TEST_PROC_AFFINITY
    /* didnt seem to work.... */
    /*
     * increasing the priority of the process to minimise packet generation
     * delay
     */
    int rc = setpriority(PRIO_PROCESS, 0, -15);

    if (rc < 0) {
        perror("setpriority:");
        fprintf(stderr, "setting priority to valid level\n");
        rc = setpriority(PRIO_PROCESS, 0, 0);
    }
    
    /* setting the affinity of the process  */
    cpu_set_t cpu_set;
    int affinity = -1;
    int ncores = 1;

    sched_getaffinity(0, sizeof(cpu_set_t), &cpu_set);
    if (errno)
        perror("couldn't get affinity:");

    if ((ncores = sysconf(_SC_NPROCESSORS_CONF)) <= 0)
        err("sysconf: couldn't get _SC_NPROCESSORS_CONF");

    CPU_ZERO(&cpu_set);
    CPU_SET(affinity, &cpu_set);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set) != 0)
        err("couldn't change CPU affinity");
#endif

    test = iperf_new_test();
    if (!test)
        iperf_errexit(NULL, "create new test error - %s", iperf_strerror(i_errno));
    iperf_defaults(test);	/* sets defaults */

    if (iperf_parse_arguments(test, argc, argv) < 0) {
        iperf_err(test, "parameter error - %s", iperf_strerror(i_errno));
        fprintf(stderr, "\n");
        usage_long(stdout);
        exit(1);
    }

    if (run(test) < 0)
        iperf_errexit(test, "error - %s", iperf_strerror(i_errno));

    iperf_free_test(test);

    return 0;
}

void *pthread_run(void *test) {
  run((struct iperf_test *)test);
  return NULL;
}

int main(int argc, char** argv) {
    struct iperf_test **tests;
    pthread_t *thread_ids;
    if (argc < 2) {
        fprintf(stderr, "USAGE: iperf server_num");
        exit(1);
    }
    if (strcmp(argv[1], "client") == 0) {
        argv[1] = "iperf";
        char new_argc = argc - 1;
        char **new_argv = calloc(sizeof(*argv), argc);
        new_argv[0] = argv[0];
        if (new_argc > 1) {
            memcpy(&new_argv[1], &argv[2], sizeof(*new_argv) * new_argc);
        }
        int r = old_main(new_argc, new_argv);
        free(new_argv);
        return r;
    }

    int server_num = atoi(argv[1]);
    printf("Launch %d servers\n", server_num);
    tests = calloc(sizeof(*tests), server_num);
    thread_ids = calloc(sizeof(*thread_ids), server_num);

    // XXX: Setting the process affinity requires root on most systems.
    //      Is this a feature we really need?
#ifdef TEST_PROC_AFFINITY
    /* didnt seem to work.... */
    /*
     * increasing the priority of the process to minimise packet generation
     * delay
     */
    int rc = setpriority(PRIO_PROCESS, 0, -15);

    if (rc < 0) {
        perror("setpriority:");
        fprintf(stderr, "setting priority to valid level\n");
        rc = setpriority(PRIO_PROCESS, 0, 0);
    }
    
    /* setting the affinity of the process  */
    cpu_set_t cpu_set;
    int affinity = -1;
    int ncores = 1;

    sched_getaffinity(0, sizeof(cpu_set_t), &cpu_set);
    if (errno)
        perror("couldn't get affinity:");

    if ((ncores = sysconf(_SC_NPROCESSORS_CONF)) <= 0)
        err("sysconf: couldn't get _SC_NPROCESSORS_CONF");

    CPU_ZERO(&cpu_set);
    CPU_SET(affinity, &cpu_set);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set) != 0)
        err("couldn't change CPU affinity");
#endif

    for (int i = 0; i < server_num; i++) {
        tests[i] = iperf_new_test();
        if (!tests[i])
          iperf_errexit(NULL, "create new test error - %s",
                        iperf_strerror(i_errno));
        iperf_defaults(tests[i]); /* sets defaults */

        optind = 0;
        int instance_argc = 4;
        char port[6];
        sprintf(port, "%d", 5201 + i);
        char *instance_argv[] = { "iperf", "-s", "-p", port, NULL};
        if (iperf_parse_arguments(tests[i], instance_argc, instance_argv) < 0) {
          iperf_err(tests[i], "parameter error - %s", iperf_strerror(i_errno));
          fprintf(stderr, "\n");
          usage_long(stdout);
          exit(1);
        }

        if (pthread_create(&thread_ids[i], NULL, pthread_run, tests[i]) != 0) {
          perror("pthread_create:\n");
          exit(1);
        }

    }
    for (int i = 0; i < server_num; i++) {
        pthread_join(thread_ids[i], NULL);
        iperf_free_test(tests[i]);
    }
}


static jmp_buf sigend_jmp_buf;

static void __attribute__ ((noreturn))
sigend_handler(int sig)
{
    longjmp(sigend_jmp_buf, 1);
}

/**************************************************************************/
static int
run(struct iperf_test *test)
{
    /* Termination signals. */
    iperf_catch_sigend(sigend_handler);
    if (setjmp(sigend_jmp_buf))
	iperf_got_sigend(test);

    /* Ignore SIGPIPE to simplify error handling */
    signal(SIGPIPE, SIG_IGN);

    switch (test->role) {
        case 's':
	    if (test->daemon) {
		int rc;
		rc = daemon(0, 0);
		if (rc < 0) {
		    i_errno = IEDAEMON;
		    iperf_errexit(test, "error - %s", iperf_strerror(i_errno));
		}
	    }
	    if (iperf_create_pidfile(test) < 0) {
		i_errno = IEPIDFILE;
		iperf_errexit(test, "error - %s", iperf_strerror(i_errno));
	    }
            for (;;) {
		int rc;
		rc = iperf_run_server(test);
		if (rc < 0) {
		    iperf_err(test, "error - %s", iperf_strerror(i_errno));
		    if (rc < -1) {
		        iperf_errexit(test, "exiting");
		    }
                }
                iperf_reset_test(test);
                if (iperf_get_test_one_off(test)) {
		    /* Authentication failure doesn't count for 1-off test */
		    if (rc < 0 && i_errno == IEAUTHTEST) {
			continue;
		    }
		    break;
		}
            }
	    iperf_delete_pidfile(test);
            break;
	case 'c':
	    if (iperf_run_client(test) < 0)
		iperf_errexit(test, "error - %s", iperf_strerror(i_errno));
            break;
        default:
            usage();
            break;
    }

    iperf_catch_sigend(SIG_DFL);
    signal(SIGPIPE, SIG_DFL);

    return 0;
}
