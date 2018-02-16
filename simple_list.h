/**
 * simple_list.h
 *
 *  Created on: 22/07/2010
 *      Author: Vinicius Kamakura
 */

/** @defgroup simple_list
  * @{
 */

#ifndef SIMPLE_LIST_H_
#define SIMPLE_LIST_H_

#include <stdint.h>


#ifdef __linux__
#include <pthread.h>
#include <sys/time.h>
#endif

// legacy structs (para nao mudar o codigo)

typedef union {
        uint32_t uint32;
        float floatdata;
        struct {
                uint8_t op;
                uint8_t reserv;
                uint16_t val;
        };
} dado_t ;

struct __attribute__ ((__packed__)) mp_serial_intdata { // mensagem padrao
        uint8_t         head;
        uint8_t         data_len;
        int16_t         type;
        uint16_t        id;

        // data
        union {
                uint32_t intdata;
                unsigned char bytedata[4];
                dado_t dado;
                float floatdata;
        };


        //uint8_t               align[1];
        uint8_t         crc8;

};



/** \brief Header comum a todos os items que a lista pode trabalhar
 *  \note Importante ser alinhado em 32bits
 */
#define COMMON_HEADER \
	void * next; \
	unsigned int ref;

struct list_item_common_header {
	COMMON_HEADER
};

/*! \brief Item com somente 1 byte de dados.
 *
 *  Usado no parsing das mensagens de entrada
 */

struct list_item_byte {
	COMMON_HEADER

	unsigned char data;
};

/*!
 * \brief Item com o pacote (pkt) que ir� integralmente pela serial e outras variaveis de controle
 */
struct list_item_msg {
	COMMON_HEADER

	struct mp_serial_intdata pkt;
	uint32_t timestamp; //!< quando foi adicionado na queue
	uint32_t lasttx;
	uint16_t tx;	//!< numero de vezes transmitido
	uint16_t maxtx;
	uint16_t txtime; //!< ms entre transmissoes
};

/*!
 * \brief Buffer, tamanho do buffer, timestamp
 */

//150k. bem mais que suficiente para um quadro 640x480 compactado em jpeg
#define ITEM_BUF_MAX_SIZE (150*1024)

struct list_item_buf {
	COMMON_HEADER

	unsigned char buf[ITEM_BUF_MAX_SIZE];
	size_t len;

	#ifdef __linux__
	struct timeval tv;
	#endif
};

/*
 * \brief Struct da lista. Tem o ponteiro para o pool de itens e outras informacoes de controle.
 */
struct list_list {
	void * head;	//!< 1o item
	int n;			//!< numero de itens
	unsigned char type; //!< tipo
	void * itempool; //!< ponteiro para a array alocada na stack/bss
	unsigned short poolsize; //!< tamanho da array

	#ifdef __linux__
	pthread_mutex_t mtx;
	#endif
};

#define LIST_UNDEF	0
#define LIST_BYTE	1
#define LIST_MSG	2
#define LIST_BUF	3

#ifdef __linux__
	#define list_lock(list) pthread_mutex_lock(&(list)->mtx)
	#define list_unlock(list) pthread_mutex_unlock(&(list)->mtx)
#else
	#define list_lock(list)
	#define list_unlock(list)
#endif

void list_init(struct list_list * list, unsigned char type, void * itempool, unsigned short size);
void list_reset(struct list_list * list);
void * list_find_avail(struct list_list *);
int list_append(struct list_list * list, void * data, void * parm);
int list_prepend(struct list_list * list, void * data, void * parm);
#define list_push list_prepend
int list_remove_first(struct list_list * list);
#define list_pop list_remove_first
int list_remove_last(struct list_list * list);
void list_dump(struct list_list * list);
void * list_get_last(struct list_list * list, void * out);
void list_test();

/**
 * @}
 */


#endif /* SIMPLE_LIST_H_ */
