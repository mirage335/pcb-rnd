#include <stdlib.h>

#ifndef RTR_MIN
#	define RTR_MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef RTR_MAX
#	define RTR_MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

RTR(privfunc) double RTR(dist2)(RTR(coord_t) x1, RTR(coord_t) y1, RTR(coord_t) x2, RTR(coord_t) y2)
{
	double dx = (x2-x1), dy = (y2-y1);
	return dx*dx + dy*dy;
}

void RTR(box_bump)(RTR(box_t) *dst, const RTR(box_t) *src)
{
	dst->x1 = RTR_MIN(dst->x1, src->x1);
	dst->y1 = RTR_MIN(dst->y1, src->y1);
	dst->x2 = RTR_MAX(dst->x2, src->x2);
	dst->y2 = RTR_MAX(dst->y2, src->y2);
}

RTR(privfunc) void RTR(recalc_bbox)(RTR(t) *node)
{
	int n;
	if (node->is_leaf) {
		node->bbox = *node->child.obj[0].box;
		for(n = 1; n < node->used; n++)
			RTR(box_bump)(&node->bbox, node->child.obj[n].box);
	}
	else {
		node->bbox = node->child.node[0]->bbox;
		for(n = 1; n < node->used; n++)
			RTR(box_bump)(&node->bbox, &node->child.node[n]->bbox);
	}
}


/* Split a leaf node into two new leaf nodes, converting the original node
   into a non-leaf node. Use a standard iterative k-means (Lloyd's) algorithm. */
RTR(privfunc) void RTR(split_leaf)(RTR(t) *node, RTR(t) **out_new1, RTR(t) **out_new2)
{
	int g, n, itc;
	int grp[RTR(size)]; /* in which group, 0 or 1, each obj is in */
	int glen[2]; /* group lengths */
	RTR(coord_t) ccx[2], ccy[2]; /* cluster centers */
	RTR(coord_t) pccx[2], pccy[2]; /* previous cluster centers */
	RTR(coord_t) ocx[RTR(size)], ocy[RTR(size)]; /* object centers */
	RTR(t) *nn[2];

	/* calculate object centers */
	for(n = 0; n < node->used; n++) {
		ocx[n] = (node->child.obj[n].box->x1 + node->child.obj[n].box->x2) / 2;
		ocy[n] = (node->child.obj[n].box->y1 + node->child.obj[n].box->y2) / 2;
	}

	/* initial centers: 1/4 and 3/4 on the topleft-bottomright line */
	pccx[0] = ccx[0] = (node->bbox.x1 + node->bbox.x2)/4;
	pccy[0] = ccy[0] = (node->bbox.y1 + node->bbox.y2)/4;
	pccx[1] = ccx[1] = 3*ccx[0];
	pccy[1] = ccy[1] = 3*ccy[0];

	for(itc = 0; itc < RTR(size); itc++) {
		/* put each object in one of the groups */
		for(n = 0; n < node->used; n++)
			grp[n] = (RTR(dist2)(ocx[n], ocy[n], ccx[0], ccy[0]) < RTR(dist2)(ocx[n], ocy[n], ccx[1], ccy[1])) ? 0 : 1;

		retry:;
		glen[0] = glen[1] = 0;
		for(n = 0; n < node->used; n++)
			glen[grp[n]]++;

		if ((glen[0] == 0) || (glen[1] == 0)) {
			/* did not manage to split reasonable, just put every second object in the other group */
			for(n = 0; n < node->used; n++)
				grp[n] = n % 2;
			goto retry;
		}


		/* recalculate the group center */
		ccx[0] = ccy[0] = ccx[1] = ccy[1] = 0;

		for(n = 0; n < node->used; n++) {
			ccx[grp[n]] += ocx[n];
			ccy[grp[n]] += ocy[n];
		}
		ccx[0] /= glen[0];
		ccy[0] /= glen[0];
		ccx[1] /= glen[1];
		ccy[1] /= glen[1];

		if ((ccx[0] == pccx[0]) && (ccy[0] == pccy[0]) && (ccx[1] == pccx[1]) && (ccy[1] == pccy[1]))
			break; /* no progress -> finished */
		pccx[0] = ccx[0];
		pccy[0] = ccy[0];
		pccx[1] = ccx[1];
		pccy[1] = ccy[1];
	}

	/*** the actual split: introduce two new nodes under 'node' ***/

	/* allocate the two new nodes */
	for(g = 0; g < 2; g++) {
		nn[g] = calloc(sizeof(RTR(t)), 1);
		nn[g]->parent = node;
		nn[g]->is_leaf = 1;
	}

	/* insert objects */
	for(n = 0; n < node->used; n++) {
		g = grp[n];
		nn[g]->child.obj[(int)nn[g]->used].box = node->child.obj[n].box;
		nn[g]->child.obj[(int)nn[g]->used].obj = node->child.obj[n].box;
		nn[g]->used++;
		nn[g]->size++;
	}

	for(g = 0; g < 2; g++)
		RTR(recalc_bbox)(nn[g]);

	/* update parent */
	node->is_leaf = 0;
	node->used = 2;
	node->child.node[0] = nn[0];
	node->child.node[1] = nn[1];

	*out_new1 = nn[0];
	*out_new2 = nn[1];
}

/* Return a penalty score based on how much box would increase the area of
   node's bbox; the smaller the value is the lower the cost is. */
RTR(privfunc) double RTR(score)(RTR(t) *node, RTR(box_t) *box)
{
	double newarea, oldarea;
	RTR(box_t) newbox;

	oldarea = (double)(node->bbox.x2 - node->bbox.x1) * (double)(node->bbox.y2 - node->bbox.y1);

	newbox.x1 = RTR_MIN(node->bbox.x1, box->x1);
	newbox.y1 = RTR_MIN(node->bbox.y1, box->y1);
	newbox.x2 = RTR_MAX(node->bbox.x2, box->x2);
	newbox.y2 = RTR_MAX(node->bbox.y2, box->y2);
	newarea = (double)(newbox.x2 - newbox.x1) * (double)(newbox.y2 - newbox.y1);

	return newarea - oldarea;
}

/* recursively increase or decrease the size of the subtree, ascending up to
   the root */
RTR(privfunc) void RTR(bump_size)(RTR(t) *node, int delta)
{
	for(;node != NULL;node = node->parent)
		node->size += delta;
}

/* recursively update the bbox up to the the root */
RTR(privfunc) void RTR(bump_bbox)(RTR(t) *node, const RTR(box_t) *with)
{
	for(;node != NULL;node = node->parent) {
		RTR(box_bump)(&node->bbox, with);
		with = &node->bbox;
	}
}

int RTR(box_contains)(const RTR(box_t) *big, const RTR(box_t) *small)
{
	if (small->x1 < big->x1) return 0;
	if (small->y1 < big->y1) return 0;
	if (small->x2 > big->x2) return 0;
	if (small->y2 > big->y2) return 0;
	return 1;
}


void RTR(insert_)(RTR(t) *dst, void *obj, RTR(box_t) *box, int force)
{
	descend:;
	if (dst->is_leaf) {
		/* Either src bbox is within dst bbox, or force is 1; add the src here */
		if (dst->used == RTR(size)) { /* if dst grew too large, split it and retry in the better half */
			RTR(t) *n1, *n2;
			RTR(split_leaf)(dst, &n1, &n2);
			if (RTR(score)(n1, box) < RTR(score)(n2, box))
				dst = n1;
			else
				dst = n2;
			force = 0;
			goto descend;
		}
		else { /* there's room in this leaf */
			dst->child.obj[(int)dst->used].box = box;
			dst->child.obj[(int)dst->used].obj = obj;
			dst->used++;
			RTR(bump_size)(dst, +1);
			if (dst->used == 1) {
				/* single-entry leaf: node bbox is child's bbox */
				dst->bbox = *box;
			}
			RTR(bump_bbox)(dst, box);
		}
	}
	else {
		int n, bestn;
		double best, curr;

		assert(dst->used > 0);

		/* insert in an object into a non-leaf node */
		if (force)
			RTR(box_bump)(&dst->bbox, box);
		/* else: box is fully within dst->bbox already */

		/* if box is encolsed within any of the existing children, descend there */
		for(n = 0; n < dst->used; n++) {
			if (RTR(box_contains)(&dst->child.node[n]->bbox, box)) {
				dst = dst->child.node[n];
				force = 0;
				goto descend;
			}
		}

		/* not within any existing box; add a new leaf, if possible */
		if (dst->child.node[0]->is_leaf && (dst->used < RTR(size))) {
			RTR(t) *newn = malloc(sizeof(RTR(t)));

			dst->child.node[(int)dst->used] = newn;
			dst->used++;

			newn->bbox = *box;
			newn->parent = dst;
			newn->is_leaf = 1;
			newn->size = 0;
			newn->used = 1;
			newn->child.obj[0].box = box;
			newn->child.obj[0].obj = obj;
			RTR(bump_size)(newn, +1);
			RTR(bump_bbox)(dst, box);
			return;
		}

		/* did not fit in any child, did not have room for a new child - have
		   to enlarge one of the existing children (pick the best one) */
		best = RTR(score)(dst->child.node[0], box);
		bestn = 0;
		for(n = 1; n < dst->used; n++) {
			curr = RTR(score)(dst->child.node[n], box);
			if (curr < best) {
				best = curr;
				bestn = n;
			}
		}
		dst = dst->child.node[bestn];
		force = 1;
		goto descend;
	}
}

void RTR(insert)(RTR(t) *dst, void *obj, RTR(box_t) *box)
{
	RTR(insert_)(dst, obj, box, 0);
}

#ifdef NDEBUG
#	define CHECK(cond) if (!(cond)) { return 1; }
#else
#	define CHECK(cond) assert(cond)
#endif

RTR(privfunc) int RTR(check_)(RTR(t) *node)
{
	int n;
	RTR(box_t) real;


	/* calculate the real bbox as it appears locally */
	if (node->is_leaf) {
		if ((node->parent == NULL) && (node->used == 0)) {
			/* special case: empty root: must be a leaf, but 0 children allowed */
			return 0;
		}
		CHECK((node->used > 0) && (node->used <= RTR(size)));
		real = *node->child.obj[0].box;
		for(n = 1; n < node->used; n++)
			RTR(box_bump)(&real, node->child.obj[n].box);
	}
	else {
		CHECK((node->used > 0) && (node->used <= RTR(size)));
		real = node->child.node[0]->bbox;
		for(n = 1; n < node->used; n++)
			RTR(box_bump)(&real, &node->child.node[n]->bbox);
	}

	/* local bbpx check */
	CHECK(real.x1 == node->bbox.x1);
	CHECK(real.x2 == node->bbox.x2);
	CHECK(real.y1 == node->bbox.y1);
	CHECK(real.y2 == node->bbox.y2);

	/* if not a leaf, recurse to check the subtree */
	if (!node->is_leaf) {
		for(n = 0; n < node->used; n++)
			if (RTR(check_)(node->child.node[n]) != 0)
				return 1;
	}

	/* all went fine, no error found */
	return 0;
}

int RTR(check)(RTR(t) *node)
{
	return RTR(check_)(node);
}


#undef CHECK

void RTR(init)(RTR(t) *root)
{
	memset(root, 0, sizeof(RTR(t)));
	root->is_leaf = 1;
}

RTR(privfunc) void RTR(free)(RTR(t) *node, int do_free_node, void *ctx, void (*leaf_free)(void *ctx, void *obj))
{
	int n;
	if (node->is_leaf) {
		if (leaf_free != NULL)
			for(n = 0; n < node->used; n++)
				leaf_free(ctx, node->child.obj[n].obj);
	}
	else {
		for(n = 0; n < node->used; n++)
			RTR(free)(node->child.node[n], 1, ctx, leaf_free);
	}
	if (do_free_node)
		free(node);
}

void RTR(uninit_free_leaves)(RTR(t) *root, void *ctx, void (*leaf_free)(void *ctx, void *obj))
{
	RTR(free)(root, 0, ctx, leaf_free);
}

void RTR(uninit)(RTR(t) *root)
{
	RTR(free)(root, 0, NULL, NULL);
}
