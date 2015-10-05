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
 * app_princ.c	(N.Panasiuk)
 * bin/vpwfetch/app_princ.c
 */

/******************************************************************************
 * %% BeginSection: includes
 */

#include "app.h"
#include "options.h"
#include "sex.h"
#include "reader.h"

#include <Bo/libc/Strings.h>
#include <Bo/libc/Stdlib.h>
#include <Bo/stralg/Strtok.h>

#include <Vodi/services/Vodiprinc.h>
#include <Vodi/services/Vodiens.h>
#include <Bo/services/Ucntl.h>

#include <Vodi/Vodilib.h>
#include <Vodi/objects/Vpwprinc.h>

#include <Bo/fundis/Seqlist.h>

#include <Bo/services/Err.h>
#define __NEED_UNIX_ERRORS
#include <Bo/Errors.h>
#include <Bo/Debug.h>

#include <Vodi/services/Vodiprinc.h>
#include <Vodi/objects/Vpwprinc.h>

#include <Bo/aorp/Aorplib.h>

/******************************************************************************
 * %% BeginSection: type/constant declarations
 */
#pragma mark -
#pragma mark [ type/constant declarations ]
#pragma mark -

struct vpw_parm_map_elem {
	char const  *app_parm_name;
	int          vpw_parm_cntl;
	u_int32_t    semantics;
};

/******************************************************************************
 * %% BeginSection: local function prototypes
 */
#pragma mark -
#pragma mark [ local function prototypes ]
#pragma mark -

static aorp_oid_t *
_t_tuple2oid(
	struct bo_sex_tuple const *aTuple,
	aorp_oid_t anOid[/* AorpK_OID_LEN */],
	struct bo_sex_uctx *anUserCtx,
	struct aorp_error *anErrPtr
	);

/******************************************************************************
 * %% BeginSection: function definitions
 */
#pragma mark -
#pragma mark [ function definitions ]
#pragma mark -

static struct vpw_parm_map_elem const _S_parm_map[] = {
	//{ "mdEnable", VodiCTL_VPW_MD_ENABLE, __myc_mk4cc('b','o','o','l') },
	//{ "mdCellSize", VodiCTL_VPW_MD_CELL_SIZE, __myc_mk4cc('u','i','n','t') },
	//{ "mdThreshold", VodiCTL_VPW_MD_THRESHOLD, __myc_mk4cc('u','i','n','t') },
	//{ "mdSquareMin", VodiCTL_VPW_MD_SQUARE_MIN, __myc_mk4cc('u','i','n','t') },

	/* Plate detector */
	{ _VPWFETCH_MAXIMAL_PLATE_SIZE, VodiCTL_VPW_PLATE_SIZE_MAX, __myc_mk4cc('u','i','n','t') },
	{ _VPWFETCH_MINIMAL_PLATE_SIZE, VodiCTL_VPW_PLATE_SIZE_MIN, __myc_mk4cc('u','i','n','t') },
	{ _VPWFETCH_IMAGE_THRESHOLD, VodiCTL_VPW_IMAGE_THRESHOLD, __myc_mk4cc('u','i','n','t') },
	{ _VPWFETCH_IMAGE_SCALE_FACTOR, VodiCTL_VPW_PLATE_IMAGE_SCALE_FACTOR, __myc_mk4cc('s','c','l','f') },
	{ _VPWFETCH_IMAGE_SIZE, VodiCTL_VPW_PLATE_IMAGE_SIZE, __myc_mk4cc('s','i','z','e') },
	{ _VPWFETCH_PLATE_FILTER_SYMCOUNT, VodiCTL_VPW_PLATE_FILTER_SYMCOUNT, __myc_mk4cc('u','i','n','t') },
	//{ "plateRofactor", VodiCTL_VPW_PLATE_FILTER_ROFACTOR, __myc_mk4cc('u','i','n','t') },
	//{ "plateRodrofactor", VodiCTL_VPW_PLATE_FILTER_RODROPFACTOR, __myc_mk4cc('u','i','n','t') },

	/* Segmentator */
	{ _VPWFETCH_IMAGE_BLUR, VodiCTL_VPW_IMAGE_BLUR, __myc_mk4cc('u','i','n','t') },

	/* Dynamica */
	{ _VPWFETCH_WITH_DYNAMIC, VodiCTL_VPW_DYNAMIC_ENABLE, __myc_mk4cc('b','o','o','l') },
	{ _VPWFETCH_COMPARABLE_TIME_MAX, VodiCTL_VPW_DYNAMIC_COMPARABLE_TIME_MAX, __myc_mk4cc('m','s','e','c') },
	{ _VPWFETCH_COMPARABLE_SYMBOLS_MIN, VodiCTL_VPW_DYNAMIC_COMPARABLE_SYMBOLS_MIN, __myc_mk4cc('u','i','n','t') },
	{ _VPWFETCH_OUTPUT_TIMEOUT, VodiCTL_VPW_DYNAMIC_OUTPUT_TIMEOUT, __myc_mk4cc('u','s','e','c') },
	{ _VPWFETCH_WITH_DUPLICATE, VodiCTL_VPW_DYNAMIC_WITH_DUPLICATE, __myc_mk4cc('b','o','o','l') },
	{ _VPWFETCH_OUTPUT_PERIOD, VodiCTL_VPW_DYNAMIC_OUTPUT_PERIOD, __myc_mk4cc('u','s','e','c') },
	{ _VPWFETCH_DURATION_WITHOUT_ACCESS, VodiCTL_VPW_DYNAMIC_DURATION_WITHOUT_ACCESS, __myc_mk4cc('u','s','e','c') },

	/* Common */
	{ _VPWFETCH_PLATE_STAR_MAX, VodiCTL_VPW_PLATE_STAR_MAX, __myc_mk4cc('u','i','n','t') },
	{ _VPWFETCH_MINIMAL_PLATE_VALIDITY, VodiCTL_VPW_PLATE_PROBABILITY_MIN, __myc_mk4cc('u','i','n','t') },
	{ _VPWFETCH_EXTRA_ANGLE_ANALYSE, VodiCTL_VPW_PLATE_EXTRA_ANGLE_ANALYSE, __myc_mk4cc('b','o','o','l') },
	{ _VPWFETCH_PRECISE_ANALYSE, VodiCTL_VPW_PLATE_PRECISE_ANALYSE, __myc_mk4cc('b','o','o','l') },
	{ _VPWFETCH_LOG_SETTINGS, VodiCTL_VPW_LOG_SETTINGS, __myc_mk4cc('b','o','o','l') },
	{ NULL }
};

static struct vpw_parm_map_elem const _S_parm_map2[] = {
	{ _VPWFETCH_TEMPLATES, VodiCTL_SETVPWI, __myc_mk4cc('c', 's', 't', 'r') },

	{ NULL }
};

__MYC_BEGIN_CDECLS

aorp_object_t
_T_app_princ_open(
	struct aorp_error *anErrPtr
	)
{
	aorp_object_t result;
	bo_status_t status;
	aorp_oid_t oidbuf[AorpK_OID_LEN], *oid;
	aorp_oid_t vpwprincoid[] = { 2, _AORP_OID_NONOP, AorpK_OBJECT_CATEGORY, 1501 };
	unsigned thrmax;
	struct vpw_parm_map_elem const *mapel;

	result = NULL;

	status = _T_get_option_with_eval_to_uint(
		&thrmax, _VPWFETCH_THREAD_MAX, _G_app_eval_ctx, anErrPtr
	);
	if (BoS_FAILURE(status)) {
		return (result);
		/* NOTREACHED */
	}

	{
		struct bo_sex_tuple *tuple;

		tuple = _T_get_option_with_eval_to_tuple(
			_VPWFETCH_PRINCIPAL, _G_app_eval_ctx, anErrPtr
		);
		if (NULL == tuple) {
			return (result);
			/* NOTREACHED */
		}

		oid = _t_tuple2oid(tuple, oidbuf, _G_app_eval_ctx, anErrPtr);

		(void)_T_sex_release((struct bo_sex_exp *)tuple, 0, NULL);

		if (NULL == oid) {
			return (result);
			/* NOTREACHED */
		}
	}

	{
		__aorp_defproinitpod_(1,(2,(vodiprinc,Vpwprinc),NULL,thrmax));
		aorp_podinit_t podinitv[] = {
			__aorp_refproinitpod_list_(1)
		};

		if (0 == AORP_OID_LCompare(oid, vpwprincoid)) {
			result = VpwprincOpen_c(
				0, NULL, podinitv, __myc_namembs(podinitv), anErrPtr
			);
		}
		else {
			result = _Art_openbyoid(
				oid, 0, NULL, podinitv, __myc_namembs(podinitv), anErrPtr
			);
		}
		if (NULL == result) {
			return (result);
			/* NOTREACHED */
		}
	}

	for (mapel = _S_parm_map; NULL != mapel->app_parm_name; ++mapel) {
		switch (mapel->semantics) {
			bo_stsbool_t boolVal;
			unsigned int uintVal;
			int intVal;
			bo_time_t timeVal;
			struct bo_sex_exp *texp;
			struct bo_sex_tuple *tuple;
			struct vodi_scale_factor sclf;
			struct vodi_size vodiSize;

		case __myc_mk4cc('b','o','o','l'):
			boolVal = _T_get_option_with_eval_to_bool(
				mapel->app_parm_name, _G_app_eval_ctx, anErrPtr
			);
			if (BoS_FAILURE(boolVal)) {
				goto L_error;
				/* NOTREACHED */
			}
			status = VodiprincSetparam(
				result, anErrPtr, mapel->vpw_parm_cntl, (unsigned)boolVal
			);
			if (BoS_FAILURE(status)) {
				goto L_error;
				/* NOTREACHED */
			}
			break;

		case __myc_mk4cc('u','i','n','t'):
			status = _T_get_option_with_eval_to_uint(
				&uintVal, mapel->app_parm_name, _G_app_eval_ctx, anErrPtr
			);
			if (BoS_FAILURE(status)) {
				goto L_error;
				/* NOTREACHED */
			}
			status = VodiprincSetparam(
				result, anErrPtr, mapel->vpw_parm_cntl, uintVal
			);
			if (BoS_FAILURE(status)) {
				goto L_error;
				/* NOTREACHED */
			}
			break;

		case __myc_mk4cc('s','i','n','t'):
			status = _T_get_option_with_eval_to_int(
				&intVal, mapel->app_parm_name, _G_app_eval_ctx, anErrPtr
			);
			if (BoS_FAILURE(status)) {
				goto L_error;
				/* NOTREACHED */
			}
			status = VodiprincSetparam(
				result, anErrPtr, mapel->vpw_parm_cntl, intVal
			);
			if (BoS_FAILURE(status)) {
				goto L_error;
				/* NOTREACHED */
			}
			break;

		case __myc_mk4cc('u','s','e','c'):
			intVal = 'u';
			goto L_case_time;
			/* NOTREACHED */

		case __myc_mk4cc('m','s','e','c'):
			intVal = 'm';
			goto L_case_time;
			/* NOTREACHED */

		case __myc_mk4cc(' ','s','e','c'):
			intVal = ' ';
			goto L_case_time;
			/* NOTREACHED */

 L_case_time:
			timeVal = _T_get_option_with_eval_to_time(
				'u', intVal, mapel->app_parm_name, _G_app_eval_ctx, anErrPtr
			);
			if (BoS_ETIME == timeVal) {
				goto L_error;
				/* NOTREACHED */
			}
			status = VodiprincSetparam(
				result, anErrPtr, mapel->vpw_parm_cntl, timeVal
			);
			if (BoS_FAILURE(status)) {
				goto L_error;
				/* NOTREACHED */
			}
			break;

		case __myc_mk4cc('s','c','l','f'):
			tuple = _T_get_option_with_eval_to_tuple(
				mapel->app_parm_name, _G_app_eval_ctx, anErrPtr
			);
			if (NULL == tuple) {
				goto L_error;
				/* NOTREACHED */
			}

			status = BoS_NORMAL;
			do {
				if (2 != _BO_SEQLIST_Size(tuple))
					break;

				texp = BO_SEQLIST_Atb(struct bo_sex_exp *, tuple, 0);
				status = _T_sex_eval_to_double(
					&sclf.sc_width, texp, _G_app_eval_ctx, anErrPtr
				);
				if (BoS_FAILURE(status))
					break;

				texp = BO_SEQLIST_Atb(struct bo_sex_exp *, tuple, 1);
				status = _T_sex_eval_to_double(
					&sclf.sc_height, texp, _G_app_eval_ctx, anErrPtr
				);
				if (BoS_FAILURE(status))
					break;

				status = VodiprincSetparam(
					result, anErrPtr, mapel->vpw_parm_cntl, &sclf
				);
			}
			while (0);

			(void)_T_sex_release((struct bo_sex_exp *)tuple, 0, NULL);

			if (BoS_FAILURE(status)) {
				goto L_error;
				/* NOTREACHED */
			}
			break;

		case __myc_mk4cc('s','i','z','e'):
			tuple = _T_get_option_with_eval_to_tuple(
				mapel->app_parm_name, _G_app_eval_ctx, anErrPtr
			);
			if (NULL == tuple) {
				goto L_error;
				/* NOTREACHED */
			}

			status = BoS_NORMAL;
			do {
				if (2 != _BO_SEQLIST_Size(tuple))
					break;

				texp = BO_SEQLIST_Atb(struct bo_sex_exp *, tuple, 0);
				status = _T_sex_eval_to_uint(
					&uintVal, texp, _G_app_eval_ctx, anErrPtr
				);
				if (BoS_FAILURE(status))
					break;
				vodiSize.sz_width = uintVal;

				texp = BO_SEQLIST_Atb(struct bo_sex_exp *, tuple, 1);
				status = _T_sex_eval_to_uint(
					&uintVal, texp, _G_app_eval_ctx, anErrPtr
				);
				if (BoS_FAILURE(status))
					break;
				vodiSize.sz_height = uintVal;

				status = VodiprincSetparam(
					result, anErrPtr, mapel->vpw_parm_cntl, &vodiSize
				);
			}
			while (0);

			(void)_T_sex_release((struct bo_sex_exp *)tuple, 0, NULL);

			if (BoS_FAILURE(status)) {
				goto L_error;
				/* NOTREACHED */
			}
			break;
		} /* switch (parm->semantics) */
	} /* foreach parm in parm-map */

	for (mapel = _S_parm_map2; NULL != mapel->app_parm_name; ++mapel) {
		switch (mapel->semantics) {
			struct bo_sex_string *sexStr;

		case __myc_mk4cc('c','s','t','r'):
			sexStr = _T_get_option_with_eval_to_string(
				mapel->app_parm_name, _G_app_eval_ctx, anErrPtr
			);
			if (NULL == sexStr) {
				goto L_error;
				/* NOTREACHED */
			}

			status = VodiprincControl(
				result, anErrPtr, mapel->vpw_parm_cntl, sexStr->str_base
			);

			(void)_T_sex_release((struct bo_sex_exp *)sexStr, 0, NULL);

			if (BoS_FAILURE(status)) {
				goto L_error;
				/* NOTREACHED */
			}
			break;
		}
	} /* foreach parm in parm-map2 */

#if 0
	status = VodiprincSetparam(result, anErrPtr, VodiCTL_VPW_LOG_SETTINGS, 1);
#endif

	status = VodiprincApplyparam(result, anErrPtr);
	if (BoS_FAILURE(status)) {
		goto L_error;
		/* NOTREACHED */
	}

	return (result);
	/* NOTREACHED */

 L_error:
	(void)AorpRelease(result, 0, NULL);
	return (NULL);
}

aorp_object_t
_T_app_princ_process(
	aorp_object_t aPrinc,
	struct vpwfetch_reader_data *aData,
	struct aorp_error *anErrPtr
	)
{
	aorp_object_t retval;
	bo_status_t status;
	vodi_vpw_options_t options = {0};
	struct vodi_ucontext user_context;
	struct vodi_image *context_image;


	assert(NULL != aPrinc);
	assert(NULL != aData);

	options.magic = VodiK_VPW_OPTIONS_WORK_MAGIC;
	options.img_seqnum = aData->rdd_seqnum;
	options.img_timestamp = aData->rdd_timestamp;

	context_image = VodiImageCreate_copy(aData->rdd_img, anErrPtr);
	user_context.uctx_change = NULLF;
	user_context.uctx_dup = NULLF;
	user_context.uctx_drelease = (vodi_udata_release_fn)VodiImageRelease;
	user_context.uctx_dretain = (vodi_udata_retain_fn)VodiImageRetain;
	user_context.uctx_udata = (bo_pointer_t)context_image;

	retval = NULL;
	status = VodiprincProcess(
		aPrinc, aData->rdd_img, 0, NULL, &user_context, NULL, &retval, &options, anErrPtr
	);
	VodiImageRelease(context_image, 0, anErrPtr);

	if (BoS_FAILURE(status)) {
		return (BoS_EPTR);
		/* NOTREACHED */
	}

	if (NULL != retval && 0 == VodiensCount(retval, NULL)) {
		(void)AorpRelease(retval, 0, NULL);
		retval = NULL;
	}

	return (retval);
}

aorp_object_t
_T_app_princ_flush(
	aorp_object_t aPrinc,
	struct aorp_error *anErrPtr
	)
{
	aorp_object_t retval;
	bo_status_t status;

	assert(NULL != aPrinc);

	retval = NULL;
	status = VodiprincFlush(aPrinc, NULL, &retval, anErrPtr);
	if (BoS_FAILURE(status)) {
		return (BoS_EPTR);
		/* NOTREACHED */
	}

	if (NULL != retval && 0 == VodiensCount(retval, NULL)) {
		(void)AorpRelease(retval, 0, NULL);
		retval = NULL;
	}

	return (retval);
}

__MYC_END_CDECLS

/******************************************************************************
 * %% BeginSection: local function prototypes
 */
#pragma mark -
#pragma mark [ local function prototypes ]
#pragma mark -

static aorp_oid_t *
_t_tuple2oid(
	struct bo_sex_tuple const *aTuple,
	aorp_oid_t anOid[],
	struct bo_sex_uctx *anUserCtx,
	struct aorp_error *anErrPtr
	)
{
	bo_status_t status;
	size_t oidc;
	unsigned oid1;
	struct bo_sex_exp *sex;

	assert(NULL != aTuple);
	assert(NULL != anOid);

#ifndef AorpL_OID_COUNT_MAX
# define AorpL_OID_COUNT_MAX AorpK_OID_LEN - 2
#endif

	oidc = _BO_SEQLIST_Size(aTuple);
	if (AorpL_OID_COUNT_MAX <= oidc) {
		goto L_error_inval;
		/* NOTREACHED */
	}

	AORP_OID_Count(anOid) = (aorp_oid_t)oidc;
	while (oidc--) {
		sex = BO_SEQLIST_Atb(struct bo_sex_exp *, aTuple, oidc);
		status = _T_sex_eval_to_uint(&oid1, sex, anUserCtx, anErrPtr);
		if (BoS_FAILURE(status)) {
			return (NULL);
			/* NOTREACHED */
		}
		if (USHRT_MAX < oid1) {
			goto L_error_inval;
			/* NOTREACHED */
		}
		AORP_OID_Field(oidc, anOid) = (aorp_oid_t)oid1;
	}

	return (anOid);
	/* NOTREACHED */

 L_error_inval:
	AorpMakeErr(
		anErrPtr,
		0,
		BoK_ArgError_InvalidArgument, EINVAL,
		2, __func__, "@tuple"
	);
	return (NULL);
}

/*
 *
 */
