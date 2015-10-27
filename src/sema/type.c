#include <ofc/sema.h>
#include <string.h>

static ofc_hashmap_t* ofc_sema_type__map = NULL;

static const char* ofc_sema_type__name[] =
{
	"LOGICAL",
	"INTEGER",
	"REAL",
	"COMPLEX",
	"BYTE",
	"CHARACTER",
	"STRCCTURE",
	"POINTER",
	"ARRAY",
};

const char* ofc_sema_type_str_rep(
	const ofc_sema_type_e type)
{
	if (type >= OFC_SEMA_TYPE_COUNT)
		return NULL;

	return ofc_sema_type__name[type];
}

static void ofc_sema_type__delete(ofc_sema_type_t* type)
{
	if (!type)
		return;

	switch (type->type)
	{
		case OFC_SEMA_TYPE_ARRAY:
			ofc_sema_array_delete(type->array);
			break;
		default:
			break;
	}

	free(type);
}

uint8_t ofc_sema_type_hash(
	const ofc_sema_type_t* type)
{
	if (!type)
		return 0;

	uint8_t hash = type->type;

	if (type->is_static   ) hash +=  8;
	if (type->is_automatic) hash += 16;
	if (type->is_volatile ) hash += 32;

	switch (type->type)
	{
		case OFC_SEMA_TYPE_STRUCTURE:
			hash += ofc_sema_structure_hash(
				type->structure);
			break;

		case OFC_SEMA_TYPE_POINTER:
			hash += ofc_sema_type_hash(
				type->subtype);
			break;

		case OFC_SEMA_TYPE_ARRAY:
			hash += ofc_sema_type_hash(
				type->subtype);
			hash += ofc_sema_array_hash(
				type->array);
			break;

		default:
			hash += type->kind;
			break;
	}

	return hash;
}

static const ofc_sema_type_t* ofc_sema_type__key(
	const ofc_sema_type_t* type)
{
	return type;
}


static const ofc_sema_type_t* ofc_sema_type__create(
	ofc_sema_type_e type,
	unsigned kind,
	ofc_sema_array_t* array,
	ofc_sema_type_t* subtype,
	const ofc_sema_structure_t* structure,
	bool is_static,
	bool is_automatic,
	bool is_volatile)
{
	if (!ofc_sema_type__map)
	{
		ofc_sema_type__map = ofc_hashmap_create(
			(void*)ofc_sema_type_hash,
			(void*)ofc_sema_type_compare,
			(void*)ofc_sema_type__key,
			(void*)ofc_sema_type__delete);
		if (!ofc_sema_type__map)
			return NULL;
	}

	ofc_sema_type_t stype =
		{
			.type         = type,
			.is_static    = is_static,
			.is_automatic = is_automatic,
			.is_volatile  = is_volatile,
		};

	switch (type)
	{
		case OFC_SEMA_TYPE_POINTER:
			stype.subtype = subtype;
			break;
		case OFC_SEMA_TYPE_ARRAY:
			stype.subtype = subtype;
			stype.array   = array;
			break;
		case OFC_SEMA_TYPE_STRUCTURE:
			stype.structure = structure;
			break;
		default:
			if (kind == 0)
			{
				/* TODO - Work this out per-kind from lang_opts. */
				kind = 4;
			}
			stype.kind = kind;
			break;
	}

	/* A LOGICAL*1 is a synonym of BYTE. */
	if ((stype.type == OFC_SEMA_TYPE_LOGICAL)
		&& (stype.kind == 1))
		stype.type = OFC_SEMA_TYPE_BYTE;

	const ofc_sema_type_t* gtype
		= ofc_hashmap_find(
			ofc_sema_type__map, &stype);
	if (gtype) return gtype;

	ofc_sema_type_t* ntype
		= (ofc_sema_type_t*)malloc(
			sizeof(ofc_sema_type_t));
	if (!ntype) return NULL;
	*ntype = stype;

	if (!ofc_hashmap_add(
		ofc_sema_type__map, ntype))
	{
		free(ntype);
		return NULL;
	}

	return ntype;
}

const ofc_sema_type_t* ofc_sema_type_create_primitive(
	ofc_sema_type_e type,
	unsigned kind,
	bool is_static,
	bool is_automatic,
	bool is_volatile)
{
	switch (type)
	{
		case OFC_SEMA_TYPE_LOGICAL:
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_REAL:
		case OFC_SEMA_TYPE_COMPLEX:
			break;
		case OFC_SEMA_TYPE_BYTE:
			if (kind > 1)
				return NULL;
			break;
		default:
			return NULL;
	}

	return ofc_sema_type__create(
		type, kind,
		NULL, NULL, NULL,
		is_static, is_automatic, is_volatile);
}

const ofc_sema_type_t* ofc_sema_type_create_byte(
	bool is_static,
	bool is_automatic,
	bool is_volatile)
{
	return ofc_sema_type_create_primitive(
		OFC_SEMA_TYPE_BYTE, 0,
		is_static, is_automatic, is_volatile);
}

const ofc_sema_type_t* ofc_sema_type_create_structure(
	const ofc_sema_structure_t* structure,
	bool is_static,
	bool is_automatic,
	bool is_volatile)
{
	return ofc_sema_type__create(
		OFC_SEMA_TYPE_STRUCTURE, 0,
		NULL, NULL, structure,
		is_static, is_automatic, is_volatile);
}

const ofc_sema_type_t* ofc_sema_type_create_pointer(
	ofc_sema_type_t* target,
	bool is_static,
	bool is_automatic,
	bool is_volatile)
{
	return ofc_sema_type__create(
		OFC_SEMA_TYPE_POINTER, 0,
		NULL, target, NULL,
		is_static, is_automatic, is_volatile);
}

const ofc_sema_type_t* ofc_sema_type_create_array(
	ofc_sema_type_t* type, ofc_sema_array_t* array,
	bool is_static,
	bool is_automatic,
	bool is_volatile)
{
	return ofc_sema_type__create(
		OFC_SEMA_TYPE_ARRAY, 0,
		array, type, NULL,
		is_static, is_automatic, is_volatile);
}



const ofc_sema_type_t* ofc_sema_type(
	const ofc_sema_scope_t* scope,
	const ofc_parse_type_t* ptype)
{
	if (!ptype)
		return NULL;


	unsigned kind = 0;

	if (ptype->params)
	{
		unsigned i;
		for (i = 0; i < ptype->params->count; i++)
		{
			/* TODO - Handle unnamed kind */
			if (ofc_str_ref_equal_strz_ci(ptype->params->call_arg[i]->name, "KIND"))
			{
				if (ptype->params->call_arg[i]->type
					!= OFC_PARSE_CALL_ARG_EXPR)
					return false;

				ofc_sema_expr_t* expr = ofc_sema_expr(
					scope, ptype->params->call_arg[i]->expr);
				if (!expr) return false;

				ofc_sema_typeval_t* tv = ofc_sema_expr_resolve(scope, expr);
				ofc_sema_expr_delete(expr);
				if (!tv) return false;

				int64_t kind64;
				bool success = ofc_sema_typeval_get_integer(tv, &kind64);
				ofc_sema_typeval_delete(tv);
				if (!success) return false;

				kind = kind64;
				if (kind != kind64)
					return false;

				if (kind == 0)
				{
					ofc_sema_scope_error(scope, ptype->src,
						"KIND must not be specified as zero");
					return false;
				}
			}
			else
			{
				ofc_sema_scope_error(scope, ptype->src,
					"Unknown parameter in type");
				return false;
			}
		}
	}

	if (ptype->kind > 0)
	{
		if (kind > 0)
		{
			if (ptype->kind == kind)
			{
				ofc_sema_scope_warning(scope, ptype->src,
					"KIND specified multiple times in type");
			}
			else
			{
				ofc_sema_scope_error(scope, ptype->src,
					"KIND specified differently in multiple places");
				return NULL;
			}
		}

		kind = ptype->kind;
	}
	else if (kind == 0)
	{
		/* TODO - If KIND is not set, get default from lang_opts. */
		kind = 4;
	}

	switch (ptype->type)
	{
		case OFC_PARSE_TYPE_DOUBLE_PRECISION:
			kind *= 2;
			break;
		case OFC_PARSE_TYPE_DOUBLE_COMPLEX:
			kind *= 2;
			break;
		default:
			break;
	}

	const ofc_sema_type_t* stype = NULL;
	switch (ptype->type)
	{
		case OFC_PARSE_TYPE_LOGICAL:
			stype = ofc_sema_type__create(
				OFC_SEMA_TYPE_LOGICAL, kind,
				NULL, NULL, NULL,
				ptype->attr.is_static,
				ptype->attr.is_automatic,
				ptype->attr.is_volatile);
			break;
		case OFC_PARSE_TYPE_CHARACTER:
			stype = ofc_sema_type__create(
				OFC_SEMA_TYPE_CHARACTER, kind,
				NULL, NULL, NULL,
				ptype->attr.is_static,
				ptype->attr.is_automatic,
				ptype->attr.is_volatile);
			break;
		case OFC_PARSE_TYPE_INTEGER:
			stype = ofc_sema_type__create(
				OFC_SEMA_TYPE_INTEGER, kind,
				NULL, NULL, NULL,
				ptype->attr.is_static,
				ptype->attr.is_automatic,
				ptype->attr.is_volatile);
			break;
		case OFC_PARSE_TYPE_REAL:
		case OFC_PARSE_TYPE_DOUBLE_PRECISION:
			stype = ofc_sema_type__create(
				OFC_SEMA_TYPE_REAL, kind,
				NULL, NULL, NULL,
				ptype->attr.is_static,
				ptype->attr.is_automatic,
				ptype->attr.is_volatile);
			break;
		case OFC_PARSE_TYPE_COMPLEX:
		case OFC_PARSE_TYPE_DOUBLE_COMPLEX:
			stype = ofc_sema_type__create(
				OFC_SEMA_TYPE_COMPLEX, kind,
				NULL, NULL, NULL,
				ptype->attr.is_static,
				ptype->attr.is_automatic,
				ptype->attr.is_volatile);
			break;
		case OFC_PARSE_TYPE_BYTE:
			stype = ofc_sema_type__create(
				OFC_SEMA_TYPE_BYTE, kind,
				NULL, NULL, NULL,
				ptype->attr.is_static,
				ptype->attr.is_automatic,
				ptype->attr.is_volatile);
			break;
		case OFC_PARSE_TYPE_TYPE:
			break;

		default:
			return NULL;
	}

	if (ptype->count_expr
		|| ptype->count_var)
	{
		/* TODO - Array from count or (LEN=?). */
		return false;
	}

	return stype;
}

bool ofc_sema_type_compare(
	const ofc_sema_type_t* a,
	const ofc_sema_type_t* b)
{
	if (!a || !b)
		return false;

	if (a == b)
		return true;

	if (a->type != b->type)
		return false;

	switch (a->type)
	{
		case OFC_SEMA_TYPE_STRUCTURE:
			return ofc_sema_structure_compare(
				a->structure, b->structure);

		case OFC_SEMA_TYPE_POINTER:
			return ofc_sema_type_compare(
				a->subtype, b->subtype);

		case OFC_SEMA_TYPE_ARRAY:
			return (ofc_sema_type_compare(
				a->subtype, b->subtype)
				&& ofc_sema_array_compare(
					a->array, b->array));

		default:
			break;
	}

	return (a->kind == b->kind);
}


unsigned ofc_sema_type_size(const ofc_sema_type_t* type)
{
	if (!type)
		return 0;

	switch (type->type)
	{
		case OFC_SEMA_TYPE_LOGICAL:
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_REAL:
			return type->kind;

		case OFC_SEMA_TYPE_COMPLEX:
			return (type->kind * 2);

		case OFC_SEMA_TYPE_BYTE:
			return 1;

		case OFC_SEMA_TYPE_STRUCTURE:
			return ofc_sema_structure_size(
				type->structure);

		case OFC_SEMA_TYPE_POINTER:
			/* TODO - Do this based on target arch. */
			return sizeof(void*);

		case OFC_SEMA_TYPE_ARRAY:
			return (ofc_sema_type_size(type->subtype)
				* ofc_sema_array_total(type->array));

		default:
			break;
	}

	return 0;
}

bool ofc_sema_type_is_integer(const ofc_sema_type_t* type)
{
	if (!type)
		return false;

	switch (type->type)
	{
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_BYTE:
			return true;
		default:
			break;
	}

	return false;
}

bool ofc_sema_type_is_logical(const ofc_sema_type_t* type)
{
	if (!type)
		return false;

	switch (type->type)
	{
		case OFC_SEMA_TYPE_LOGICAL:
		case OFC_SEMA_TYPE_BYTE:
			return true;
		default:
			break;
	}

	return false;
}



static unsigned umax(unsigned a, unsigned b)
	{ return (a > b ? a : b); }

const ofc_sema_type_t* ofc_sema_type_promote(
	const ofc_sema_type_t* a,
	const ofc_sema_type_t* b)
{
	if (!a) return b;
	if (!b) return a;

	if (ofc_sema_type_compare(a, b))
		return a;

	if (a->type == b->type)
		return (a->kind > b->kind ? a : b);

	/* BYTE is always promoted. */
	if (a->type == OFC_SEMA_TYPE_BYTE)
		return b;
	if (b->type == OFC_SEMA_TYPE_BYTE)
		return a;

	bool logical = ((a->type == OFC_SEMA_TYPE_LOGICAL)
		|| (b->type == OFC_SEMA_TYPE_LOGICAL));
	bool integer = ((a->type == OFC_SEMA_TYPE_INTEGER)
		|| (b->type == OFC_SEMA_TYPE_INTEGER));
	bool real = ((a->type == OFC_SEMA_TYPE_REAL)
		|| (b->type == OFC_SEMA_TYPE_REAL));
	bool complex = ((a->type == OFC_SEMA_TYPE_COMPLEX)
		|| (b->type == OFC_SEMA_TYPE_COMPLEX));

	unsigned kind = umax(a->kind, b->kind);

	/* Promoted types ignore decl attributes. */

	if (logical && integer)
	{
		return ofc_sema_type_create_primitive(
			OFC_SEMA_TYPE_INTEGER, kind,
			false, false, false);
	}
	else if (real && (logical || integer))
	{
		return ofc_sema_type_create_primitive(
			OFC_SEMA_TYPE_REAL, kind,
			false, false, false);
	}
	else if (complex && (real || logical || integer))
	{
		return ofc_sema_type_create_primitive(
			OFC_SEMA_TYPE_COMPLEX, kind,
			false, false, false);
	}

	/* We can't promote characters, arrays, structures or pointers. */

	return NULL;
}