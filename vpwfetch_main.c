/* -*- Mode: c; tab-width: 4; -*-
 * -*- ex: ts=4 -*-
 */

/*
 * Copyright 2007 VIT Ltd. All rights reserved.
 * VIT PROPRIETARY/CONFIDENTIAL.
 *
 * $Id$
 */

/*
 * main.c	(N.Panasiuk)
 * bin/vpwfetch/mian.c
 */

/******************************************************************************
 * %% BeginSection: includes
 */

#include "app.h"
#include "options.h"
#include "sex.h"

#include <Bo/libc/Stdio.h>
#include <Bo/libc/Time.h>
#include <Bo/libc/Stdlib.h>
	/* setenv */

#include <Bo/memman/BoMalloc.h>
#include <Bo/fundis/Seqlist.h>

#include <Bo/services/Ucntl.h>

#include <Bo/services/Err.h>
#define __NEED_UNIX_ERRORS
#include <Bo/Errors.h>
#include <Bo/Debug.h>
#include <Bo/services/Ccsconv.h>

#include <Vodi/Vodilib.h>
#include <Vodi/objects/Vpwprinc.h>
#include <Vodi/services/Ucntl.h>
#include <Vodi/services/Vodiprinc.h>
#include <Vodi/services/Vodiens.h>
#include <Vodi/services/Vodires.h>
#include <Vodi/utils/arrio/Filepath.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <opencv2/highgui/highgui_c.h>

#define LOCKFILE "/var/run/lock/vitstreaming.lock"
#define IMWIDTH 1920
#define IMHEIGHT 1080

#ifdef BO_SYSTYPE_WIN32
# define setenv(x,y,z) _putenv_s(x,y)
#endif

/******************************************************************************
 * %% BeginSection: local function prototypes
 */
#pragma mark -
#pragma mark [ local function prototypes ]
#pragma mark -

static bo_status_t
_t_main_loop(
	struct aorp_error *anErrPtr
	);

/******************************************************************************
 * %% BeginSection: function definitions
 */
#pragma mark -
#pragma mark [ function definitions ]
#pragma mark -

static int getwidth()
{
	int width;
	if (NULL == getenv("VIT_WIDTH")) {
                width = IMWIDTH;
        }
        else {
                width = atoi(getenv("VIT_WIDTH"));
        }
	return width;
}

static int getheight()
{
	int height;
	if (NULL == getenv("VIT_HEIGHT")) {
                height = IMHEIGHT;
        }
        else {
                height = atoi(getenv("VIT_HEIGHT"));
        }
	return height;
}

int VPWFETCH_MAIN(int argc, char *argv[])
{
	bo_status_t status;
	char const *configFile;
	struct aorp_error *errp;
	struct aorp_error256 err256 = {0};

	_AORP_ERROR256_Init(&err256);
	errp = (struct aorp_error *)&err256;

	configFile = _VPWFETCH_GLOBAL_CONFIG_FILE;
	status = _T_load_config(configFile, errp);
	if (BoS_FAILURE(status) &&
		BoK_FileError_NoSuchEntry != _AORP_ERROR_Code(errp))
	{
		goto L_error;
		/* NOTREACHED */
	}

	status = _T_parseopt(argc, argv, errp);
	if (BoS_FAILURE(status)) {
		goto L_error;
		/* NOTREACHED */
	}

	if (_BoS_HELP == status) {
		_T_usage();
		return (0);
		/* NOTREACHED */
	}

	status = _T_app_load_requireds(errp);
	if (BoS_FAILURE(status)) {
		goto L_error;
		/* NOTREACHED */
	}

	status = _t_main_loop(errp);
	if (BoS_FAILURE(status)) {
		goto L_error;
		/* NOTREACHED */
	}

	return (0);
	/* NOTREACHED */

 L_error:
	fprintf(stderr, "%s\n", errp->msg);
	return (1);
}

/******************************************************************************
 * %% BeginSection: local function definitions
 */
#pragma mark -
#pragma mark [ local function definitions ]
#pragma mark -

static bo_status_t
_t_main_loop(
	struct aorp_error *anErrPtr
	)
{
	bo_status_t status;
	struct vpwfetch_eval_data *evaldata;
	struct vpwfetch_app_reader readerMemory, *reader;
	struct vpwfetch_reader_data readerData, *rdata;
	aorp_object_t princ;
	aorp_object_t ens;
	bo_seqlist_t enslist;
	u_int32_t groupSeqnum;
	_Bool newGroup;

	int stream_pid;
	int md;
	FILE *fh;

	bo_utime_t t00, t01, t02, t03;
	bo_usec_t delta;
	unsigned framec;
	u_int64_t totalFrames;

	evaldata = _G_app_eval_ctx->user_data;
	reader = NULL;
	princ = NULL;
	_BO_SEQLIST_Init(&enslist);

	reader = _T_app_reader_open(&readerMemory, anErrPtr);
	if (NULL == reader) {
		status = BoS_ERR;
		goto L_error;
		/* NOTREACHED */
	}

	/*
	 * Output to header file
	 */
	evaldata->ofile = "${"_VPWFETCH_HEADER_FILE"}";
	status = _T_app_write_v2(
		_VPWFETCH_HEADER_FILE,
		_VPWFETCH_HEADER_LINE,
		_VPWFETCH_INPUT_ENCODING,
		_VPWFETCH_OUTPUT_ENCODING,
		anErrPtr
	);
	evaldata->ofile = NULL;

	if (BoS_FAILURE(status)) {
		goto L_error;
		/* NOTREACHED */
	}

	rdata = &readerData;
	groupSeqnum = 0;

	t00 = BoTime(NULL, NULL);
	totalFrames = 0;

	t01 = t00;
	framec = 0;

	/* Find streamer PID */
	fh = fopen(LOCKFILE, "r");
	if (NULL != fh) {
		fscanf(fh, "%d", &stream_pid);
		fclose(fh);
		if (kill(stream_pid, 0)) {
			printf("Streaming client not found\n");
			goto L_error;
		}
	}
	else {
		printf("Streaming client not found\n");
		goto L_error;
	}

	for (;;) {
		status = _T_app_reader_grab(reader, rdata, anErrPtr);
		if (!BoS_DOBRE(status))
			break;

		newGroup = (groupSeqnum != rdata->rdd_group_seqnum);
		groupSeqnum = rdata->rdd_group_seqnum;

		evaldata->rdata = rdata;

		/*
		 * Output to prefix file
		 */
		evaldata->ofile = "${"_VPWFETCH_PREFIX_FILE"}";
		status = _T_app_write_v2(
			_VPWFETCH_PREFIX_FILE,
			_VPWFETCH_PREFIX_LINE,
			_VPWFETCH_INPUT_ENCODING,
			_VPWFETCH_OUTPUT_ENCODING,
			anErrPtr
		);
		evaldata->ofile = NULL;

		if (BoS_FAILURE(status)) {
			goto L_error;
			/* NOTREACHED */
		}

		if (NULL == princ) {
			princ = _T_app_princ_open(anErrPtr);
			if (NULL == princ) {
				status = BoS_ERR;
				goto L_error;
				/* NOTREACHED */
			}
		}

		setenv("VPW_SRCIMAGE_PATH", rdata->rdd_path, 1);

		evaldata->process_duration = BoTime(NULL, NULL);

		ens = _T_app_princ_process(princ, rdata, anErrPtr);

		evaldata->process_duration = (
			BoTime(NULL, NULL) - evaldata->process_duration
		);

		if (BoS_EPTR == ens) {
			status = BoS_ERR;
			goto L_error;
			/* NOTREACHED */
		}

		status = _T_app_ensemble_process(ens, &enslist, princ, newGroup, anErrPtr);
		{
			struct CvMat img;
			CvScalar sc;
			CvFont fnt;
			aorp_object_t result;
			struct vodi_result_info *info;
			struct vodi_vpw_result_info plateinfo;
			struct vodi_plate_info_spec *pis;
			struct vodi_plate *plate;
			struct aorp_ccscvtor cconv;
			char plateStr[100];
			char fpsstr[100];
			int wb;

			if (NULL == ens) goto L_DISPLAY;
			status = VodiensGet(ens, 0, 0, 1, &result, NULL);
			if (BoS_DOBRE(status)) {
				info = (struct vodi_result_info *)&plateinfo;
				VODI_RESULT_INFO_TYPE(info) = VodiK_VPW_RESULT_INFO;
				status = VodiresFetchinfo(result, info, NULL);
				if (BoS_SUCCESS(status)) {
					pis = VODI_RESULT_INFO_SPEC(info, plate);
					plate = &pis->pis_plate_variantv[0];
					status = AorpOpenCcscvtor_i_(BoK_UNK_CSET, BoK_UTF8_CSET,
						&cconv, NULL);
					if (BoS_SUCCESS(status)) {
						AorpCcsconv_s_2mb(&cconv, plate->pv_plate_string,
							plateStr, NULL);
						AorpCcscvtorClose(&cconv, NULL);
					}
					VodiResultInfoDestroy(info);
				}
			}
 L_DISPLAY:
			sc.val[0] = 0; sc.val[1] = 0; sc.val[2] = 0; sc.val[3] = 0;
			cvInitMatHeader(&img, getheight(), getwidth(), CV_8UC1,
				rdata->rdd_img->img_base, CV_AUTOSTEP);
			cvInitFont(&fnt, CV_FONT_HERSHEY_SIMPLEX, 3.0f, 3.0f, 0.0f, 5, 8);
			sprintf(fpsstr, "%.2f fps", evaldata->fps);
			cvPutText(&img, plateStr, cvPoint(100, 100), &fnt, sc);
			cvPutText(&img, fpsstr, cvPoint(100, 175), &fnt, sc);
			md = shm_open("vit_image", O_CREAT | O_RDWR,  S_IRUSR | S_IWUSR |
				S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
			wb = write(md, rdata->rdd_img->img_base, getwidth() * getheight());
			printf("Wrote %d bytes\n");
			fsync(md);
			close(md);
			kill(stream_pid, SIGUSR1);
		}


		if (NULL != ens)
			(void)AorpRelease(ens, 0, NULL);

		if (BoS_FAILURE(status)) {
			goto L_error;
			/* NOTREACHED */
		}

		/*
		 * Output to sufix file
		 */
		evaldata->ofile = "${"_VPWFETCH_SUFIX_FILE"}";
		status = _T_app_write_v2(
			_VPWFETCH_SUFIX_FILE,
			_VPWFETCH_SUFIX_LINE,
			_VPWFETCH_INPUT_ENCODING,
			_VPWFETCH_OUTPUT_ENCODING,
			anErrPtr
		);
		evaldata->ofile = NULL;

		if (BoS_FAILURE(status)) {
			goto L_error;
			/* NOTREACHED */
		}

		t02 = BoTime(NULL, NULL);
		delta = t02 - t01;
		++framec;
		++totalFrames;

		if (1000000 <= delta) {
			evaldata->fps = (1000000.0 * framec) / delta;
			framec = 0;
			t01 = t02;
		}
	} /* foreach image */
	if (BoS_FAILURE(status)) {
		goto L_error;
		/* NOTREACHED */
	}

	evaldata->rdata = NULL;

	if (NULL != princ) {
		ens = _T_app_princ_flush(princ, anErrPtr);
		if (BoS_EPTR == ens) {
			status = BoS_ERR;
			goto L_error;
			/* NOTREACHED */
		}

		status = _T_app_ensemble_process(ens, &enslist, princ, true, anErrPtr);

		if (NULL != ens)
			(void)AorpRelease(ens, 0, NULL);

		if (BoS_FAILURE(status)) {
			goto L_error;
			/* NOTREACHED */
		}
	}

	t03 = BoTime(NULL, NULL);
	evaldata->avgfps = (1000000.0 * totalFrames) / (t03 - t00);

	/*
	 * Output to footer file
	 */
	evaldata->ofile = "${"_VPWFETCH_FOOTER_FILE"}";
	status = _T_app_write_v2(
		_VPWFETCH_FOOTER_FILE,
		_VPWFETCH_FOOTER_LINE,
		_VPWFETCH_INPUT_ENCODING,
		_VPWFETCH_OUTPUT_ENCODING,
		anErrPtr
	);
	evaldata->ofile = NULL;

	if (BoS_FAILURE(status)) {
		goto L_error;
		/* NOTREACHED */
	}

	status = BoS_NORMAL;

 L_error:
	evaldata->rdata = NULL;

	BO_SEQLIST_Destroy_x_(aorp_object_t, &enslist,
		{ (void)AorpRelease(*_$1, 0, NULL); }
	);

	if (NULL != princ)
		(void)AorpRelease(princ, 0, NULL);

	(void)_T_app_reader_close(reader, NULL);

	return (status);
}

/*
 *
 */
