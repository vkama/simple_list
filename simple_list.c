/*!
 * simple_list.c
 *
 * @file 	simple_list.c
 * @brief 	Codigo para single linked list (sem alocação dinamica)
 *
 * @version	1.2
 * @date	21/07/2010
 * @author	Vinicius Kamakura
 *
 *  A ideia é criar uma pequena lib de linked list, sem alocação dinamica usando um pool de itens fixo
 *  Não thread safe.
 *
 *  v1.1 - adicionado mutex, list_lock e list_unlock no linux (vk)
 *  v1.2 - list_buf agora usa timestamp no linux (vk)
 *
 *	todo: adicionar prev na struct? com isso seria possivel tirar itens do meio de uma forma elegante
 *	todo: adicionar chamada de reset a init?
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "simple_list.h"

void list_init(struct list_list * list, unsigned char type, void * itempool, unsigned short poolsize) {

	list->itempool = itempool;
	list->type = type;
	list->poolsize = poolsize;

	#ifdef __linux__
	pthread_mutex_init(&list->mtx, NULL);
	#endif

	list_reset(list);
}


void list_reset(struct list_list * list) {

	list->head = NULL;
	list->n = 0;

	switch (list->type) {
	case LIST_BYTE:
		memset(list->itempool, 0, sizeof(struct list_item_byte)*list->poolsize);
		break;
	case LIST_MSG:
		memset(list->itempool, 0, sizeof(struct list_item_msg)*list->poolsize);
		break;
	case LIST_BUF:
		memset(list->itempool, 0, sizeof(struct list_item_buf)*list->poolsize);
		break;
	default:
		fprintf(stderr, "list_reset(): tipo %d nao existe\n\r", list->type);
		break;

	}

}

/*! encontra um item do pool disponivel (ref == 0) */
/* retorna NULL caso nao exista um disponivel */
void * list_find_avail(struct list_list * list) {
	int i;

	// talvez se adicionarmos um campo em lista com o tamanho de item, poderiamos unificar esta funcao?
	if (list->type == LIST_BYTE) {
		struct list_item_byte * p = list->itempool;
		for (i = 0; i < list->poolsize; i++) {
			if (p[i].ref == 0)
				return &p[i];
		}
	}
	else if (list->type == LIST_MSG) {
		struct list_item_msg * p = list->itempool;
		for (i = 0; i < list->poolsize; i++) {
			if (p[i].ref == 0)
				return &p[i];
		}
	}
	else if (list->type == LIST_BUF) {
		struct list_item_buf * p = list->itempool;
		for (i = 0; i < list->poolsize; i++) {
			if (p[i].ref == 0)
				return &p[i];
		}
	}
	else {
		fprintf(stderr, "%s: lista tipo %d nao existe\n\r", __FUNCTION__ , list->type);
	}

	return NULL;
}

int list_append(struct list_list * list, void * data, void * parm) {
	struct list_item_common_header * item;
	struct list_item_common_header * oldp;

	//void * oldp;

	if (list == NULL || data == NULL)
		return -1;

	item = list_find_avail(list);

	if (item == NULL) {
		return -2;
	}


	if (list->type == LIST_BYTE) {
		//monta item
		struct list_item_byte * p = (struct list_item_byte *)item;
		//((struct list_item_byte *)(item))->data = *(unsigned char *)data;
		p->data = *(unsigned char *)data;

		p->ref++;
		p->next = NULL;
	}
	else if (list->type == LIST_MSG) {
		struct list_item_msg * p = (struct list_item_msg *)item;
		memcpy(p, data, sizeof(struct list_item_msg));

		p->ref++;
		p->next = NULL;
	}
	else if (list->type == LIST_BUF) {
		if (parm == NULL) {
			return -4;
		}

		size_t buflen = *(size_t *)parm;

		if (buflen > ITEM_BUF_MAX_SIZE) {
			return -5;
		}

		struct list_item_buf * p = (struct list_item_buf *)item;
		memcpy(p->buf, data, buflen);

		p->len = buflen;
		p->ref++;
		p->next = NULL;

		#ifdef __linux__
		gettimeofday(&p->tv, NULL);
		#endif
	}
	else {
		fprintf(stderr, "list_append(): lista tipo %d nao existe\n\r", list->type);
		return -3;
	}

	oldp = list->head;

	if (oldp == NULL) { // head == NULL, lista vazia
		list->head = item;
		list->n++;
		return 0;
	}

	while (oldp->next != NULL)
		oldp = oldp->next;

	oldp->next = item;

	list->n++;

	return 0;
}

int list_prepend(struct list_list * list, void * data, void * parm) {

	void * item;
	void * oldhead;

	if (list == NULL || data == NULL)
		return -1;

	if (list->head == NULL)	// lista vazia, entao da um append
		return list_append(list, data, NULL);

	item = list_find_avail(list);

	if (item == NULL) {
		return -2;
	}

	oldhead = list->head;

	if (list->type == LIST_BYTE) {
		struct list_item_byte * p = item;

		p->data = *(unsigned char *)data;
		p->ref++;
		p->next = oldhead;
	}
	else if (list->type == LIST_MSG) {
		struct list_item_msg * p = (struct list_item_msg *)item;

		memcpy(p, data, sizeof(struct list_item_msg));
		p->ref++;
		p->next = oldhead;
	}
	else if (list->type == LIST_BUF) {
		if (parm == NULL) {
			return -4;
		}

		size_t buflen = *(size_t *)parm;

		if (buflen > ITEM_BUF_MAX_SIZE) {
			return -5;
		}

		struct list_item_buf * p = (struct list_item_buf *)item;
		memcpy(p->buf, data, buflen);

		p->len = buflen;
		p->ref++;
		p->next = oldhead;

		#ifdef __linux__
		gettimeofday(&p->tv, NULL);
		#endif

	}
	else {
		fprintf(stderr, "%s: lista tipo %d nao existe\n\r", __FUNCTION__ , list->type);
		return -3;
	}

	list->head = item;
	list->n++;

	return 0;
}

/* todo: erro aqui? tive q mudar pra ref = 0 ao inves de ref--, talvez o erro esteja em outro lugar, nao decrementando */
int list_remove_first(struct list_list * list) {

	struct list_item_common_header * oldhead;

	if (list == NULL)
		return -1;

	if (list->head == NULL)
		return -2;

	oldhead = list->head;

	if (oldhead->next) {
		list->head = oldhead->next;
	}
	else {
		list->head = NULL;
	}

	//limpa e marca como disponivel
	oldhead->next = NULL;
	oldhead->ref = 0;

	list->n--;

	return 0;
}

int list_remove_last(struct list_list * list) {
	struct list_item_common_header * p;
	struct list_item_common_header * prev = NULL;

	if (list == NULL)
		return -1;

	if (list->head == NULL)
		return -2;

	p = list->head;

	while (p->next) {
		prev = p;
		p = p->next;
	}

	p->next = NULL;
	p->ref--;

	if (prev)
		prev->next = NULL;

	list->n--;

	return 0;
}

void list_dump(struct list_list * list) {
	if (list->type == LIST_BYTE) {
		struct list_item_byte * item;

		fprintf(stderr, "list items: %d max poolsize: %d\n\r", list->n, list->poolsize);

		item = list->head;

		if (item == NULL)
			return;

		while (item != NULL) {
			fprintf(stderr, "item data: %d (item addr: %p)\n\r", item->data, item);
			item = item->next;
		}
	}
	else if (list->type == LIST_MSG) {
		struct list_item_msg * item;

		fprintf(stderr, "list items: %d max poolsize: %d\n\r", list->n, list->poolsize);

		item = list->head;

		if (item == NULL)
			return;

		while (item != NULL) {
			fprintf(stderr, "{\n\r\tts: %d\n\r\tlasttx: %d\n\r\tmaxtx: %d\n\r\ttx: %d\n\r\ttxtime: %d\n\r}\n\r", item->timestamp, item->lasttx, item->maxtx, item->tx, item->txtime);
			//pktdump_lite("", &item->pkt);
			item = item->next;
		}
	}
	else if (list->type == LIST_BUF) {
		struct list_item_buf * item;

		fprintf(stderr, "list items: %d max poolsize: %d\n\r", list->n, list->poolsize);

		item = list->head;

		if (item == NULL)
			return;

		while (item != NULL) {
			fprintf(stderr, "{\n\r\tlen: %d\n\r\tts: %d\n\r\tpointer: %p\n\r}\n\r", item->len, item->tv.tv_sec, item->buf);
			item = item->next;
		}
	}
	else
		fprintf(stderr, "%s: lista tipo %d nao existe\n\r", __FUNCTION__ , list->type);
}

void * list_get_last(struct list_list * list, void * out) {

	struct list_item_common_header * p;
	int i;

	p = list->head;

	if (!p)
		return NULL;

	while (p->next)
		p = p->next;

	if (out) {
		switch (list->type) {
		case LIST_BYTE:
			i = sizeof(struct list_item_byte);
			break;
		case LIST_MSG:
			i = sizeof(struct list_item_msg);
			break;
		default:
			i = 0;
			break;
		}

		memcpy(out, p, i);
	}

	return p;

}

/*
 * saida:
 *
init done
append done
list items: 5 max poolsize: 5
item data: 123 (item addr: 0x10007d04)
item data: 124 (item addr: 0x10007d10)
item data: 125 (item addr: 0x10007d1c)
item data: 126 (item addr: 0x10007d28)
item data: 127 (item addr: 0x10007d34)
prepend done
list items: 5 max poolsize: 5
item data: 4 (item addr: 0x10007d34)
item data: 3 (item addr: 0x10007d28)
item data: 2 (item addr: 0x10007d1c)
item data: 1 (item addr: 0x10007d10)
item data: 0 (item addr: 0x10007d04)
list items: 5 max poolsize: 5
item data: 255 (item addr: 0x10007d28)
item data: 2 (item addr: 0x10007d1c)
item data: 1 (item addr: 0x10007d10)
item data: 0 (item addr: 0x10007d04)
item data: 100 (item addr: 0x10007d34)
 *
 */
void list_test(void) {
	char i;
	struct list_list fifo;
	struct list_item_byte items[5];

	list_init(&fifo, LIST_BYTE, items, 5);
	list_reset(&fifo);

	fprintf(stderr, "init done\n\r");

	i = 123;
	while (list_append(&fifo, &i, NULL) == 0)
		i++;

	fprintf(stderr, "append done\n\r");

	list_dump(&fifo);
	list_reset(&fifo);

	i = 0;
	while (list_prepend(&fifo, &i, NULL) == 0)
		i++;

	fprintf(stderr, "prepend done\n\r");

	list_dump(&fifo);

	list_remove_first(&fifo);
	list_remove_first(&fifo);
	i = 255;
	list_prepend(&fifo, &i, NULL);
	i = 99;
	list_append(&fifo, &i, NULL);
	i = 123; // nao tem q aparecer
	list_append(&fifo, &i, NULL);
	list_remove_last(&fifo);
	i = 100;
	list_append(&fifo, &i, NULL);
	list_dump(&fifo);

	/* msg */

	struct list_list serial_msg;
	struct list_item_msg msgs[5];
	struct list_item_msg tmpmsg;
	uint16_t id = 123;

	memset(&tmpmsg, 0, sizeof(tmpmsg));
	list_init(&serial_msg, LIST_MSG, msgs, 5);
	fprintf(stderr, "init msg done\n\r");

	do {
		tmpmsg.pkt.id = id++;
		tmpmsg.timestamp = time(NULL);
		tmpmsg.pkt.intdata = time(NULL);

	} while (list_append(&serial_msg, &tmpmsg, NULL) == 0);
	fprintf(stderr, "append done\n\r");

	list_dump(&serial_msg);
	list_reset(&serial_msg);

	id = 1;
	do {
		tmpmsg.pkt.id = id++;
		tmpmsg.timestamp = time(NULL);
		tmpmsg.pkt.intdata = time(NULL);

	} while (list_prepend(&serial_msg, &tmpmsg, NULL) == 0);
	fprintf(stderr, "prepend done\n\r");

	list_dump(&serial_msg);

	list_remove_first(&serial_msg);
	list_remove_first(&serial_msg);

	tmpmsg.pkt.id = 255;
	tmpmsg.pkt.intdata = time(NULL);
	list_prepend(&serial_msg, &tmpmsg, NULL);

	tmpmsg.pkt.id = 99;
	tmpmsg.pkt.intdata = time(NULL);
	list_append(&serial_msg, &tmpmsg, NULL);

	tmpmsg.pkt.id = 123; // nao pode aparecer
	tmpmsg.pkt.intdata = time(NULL);
	list_append(&serial_msg, &tmpmsg, NULL);

	list_remove_last(&serial_msg);

	tmpmsg.pkt.id = 100; // nao pode aparecer
	tmpmsg.pkt.intdata = time(NULL);
	list_append(&serial_msg, &tmpmsg, NULL);

	list_dump(&serial_msg);

	list_remove_last(&serial_msg);
	list_get_last(&serial_msg, &tmpmsg);

	fprintf(stderr, "--\n\r");
	//pktdump("", &tmpmsg.pkt);
}
