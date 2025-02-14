/** @file moal_shim.c
  *
  * @brief This file contains the callback functions registered to MLAN
  *
  *
  * Copyright 2014-2021 NXP
  *
  * This software file (the File) is distributed by NXP
  * under the terms of the GNU General Public License Version 2, June 1991
  * (the License).  You may use, redistribute and/or modify the File in
  * accordance with the terms and conditions of the License, a copy of which
  * is available by writing to the Free Software Foundation, Inc.,
  * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
  * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
  *
  * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
  * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
  * this warranty disclaimer.
  *
  */

/********************************************************
Change log:
    10/21/2008: initial version
********************************************************/

#include	"moal_main.h"
#include	"moal_sdio.h"
#ifdef UAP_SUPPORT
#include    "moal_uap.h"
#endif
#if defined(STA_CFG80211) || defined(UAP_CFG80211)
#include "moal_cfg80211.h"
#include "moal_cfgvendor.h"
#endif
extern int drv_mode;
#include <asm/div64.h>

#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
extern int host_mlme;
#endif

/********************************************************
		Local Variables
********************************************************/
/** moal_lock */
typedef struct _moal_lock {
	/** Lock */
	spinlock_t lock;
	/** Flags */
	unsigned long flags;
} moal_lock;

/********************************************************
		Global Variables
********************************************************/
extern int cfg80211_wext;

extern int hw_test;

#ifdef ANDROID_KERNEL
extern int wakelock_timeout;
#endif
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
#if defined(STA_CFG80211) || defined(UAP_CFG80211)
extern int dfs_offload;
#endif
#endif

/** napi support*/
extern int napi;

/********************************************************
		Local Functions
********************************************************/

/********************************************************
		Global Functions
********************************************************/
/**
 *  @brief Alloc a buffer
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param size     The size of the buffer to be allocated
 *  @param flag     The type of the buffer to be allocated
 *  @param ppbuf    Pointer to a buffer location to store buffer pointer allocated
 *
 *  @return         MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_malloc(IN t_void *pmoal_handle,
	    IN t_u32 size, IN t_u32 flag, OUT t_u8 **ppbuf)
{
	moal_handle *handle = (moal_handle *)pmoal_handle;
	t_u32 mem_flag = (in_interrupt() || irqs_disabled())?
		GFP_ATOMIC : GFP_KERNEL;

	if (flag & MLAN_MEM_DMA)
		mem_flag |= GFP_DMA;

	*ppbuf = kzalloc(size, mem_flag);
	if (*ppbuf == NULL) {
		PRINTM(MERROR, "%s: allocate memory (%d bytes) failed!\n",
		       __func__, (int)size);
		return MLAN_STATUS_FAILURE;
	}
	atomic_inc(&handle->malloc_count);

	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Free a buffer
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param pbuf     Pointer to the buffer to be freed
 *
 *  @return         MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_mfree(IN t_void *pmoal_handle, IN t_u8 *pbuf)
{
	moal_handle *handle = (moal_handle *)pmoal_handle;

	if (!pbuf)
		return MLAN_STATUS_FAILURE;
	kfree(pbuf);
	atomic_dec(&handle->malloc_count);
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Alloc a vitual-address-continuous buffer
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param size     The size of the buffer to be allocated
 *  @param ppbuf    Pointer to a buffer location to store buffer pointer allocated
 *
 *  @return         MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_vmalloc(IN t_void *pmoal_handle, IN t_u32 size, OUT t_u8 **ppbuf)
{
	moal_handle *handle = (moal_handle *)pmoal_handle;

	*ppbuf = vmalloc(size);
	if (*ppbuf == NULL) {
		PRINTM(MERROR, "%s: vmalloc (%d bytes) failed!", __func__,
		       (int)size);
		return MLAN_STATUS_FAILURE;
	}
	atomic_inc(&handle->vmalloc_count);

	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Free a buffer allocated by vmalloc
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param pbuf     Pointer to the buffer to be freed
 *
 *  @return         MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_vfree(IN t_void *pmoal_handle, IN t_u8 *pbuf)
{
	moal_handle *handle = (moal_handle *)pmoal_handle;

	if (!pbuf)
		return MLAN_STATUS_FAILURE;
	vfree(pbuf);
	atomic_dec(&handle->vmalloc_count);
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Fill memory with constant byte
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param pmem     Pointer to the memory area
 *  @param byte     A constant byte
 *  @param num      Number of bytes to fill
 *
 *  @return         Pointer to the memory area
 */
t_void *
moal_memset(IN t_void *pmoal_handle,
	    IN t_void *pmem, IN t_u8 byte, IN t_u32 num)
{
	t_void *p = pmem;

	if (pmem && num)
		p = memset(pmem, byte, num);

	return p;
}

/**
 *  @brief Copy memory from one area to another
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param pdest    Pointer to the dest memory
 *  @param psrc     Pointer to the src memory
 *  @param num      Number of bytes to move
 *
 *  @return         Pointer to the dest memory
 */
t_void *
moal_memcpy(IN t_void *pmoal_handle,
	    IN t_void *pdest, IN const t_void *psrc, IN t_u32 num)
{
	t_void *p = pdest;

	if (pdest && psrc && num)
		p = memcpy(pdest, psrc, num);

	return p;
}

/**
 *  @brief Move memory from one area to another
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param pdest    Pointer to the dest memory
 *  @param psrc     Pointer to the src memory
 *  @param num      Number of bytes to move
 *
 *  @return         Pointer to the dest memory
 */
t_void *
moal_memmove(IN t_void *pmoal_handle,
	     IN t_void *pdest, IN const t_void *psrc, IN t_u32 num)
{
	t_void *p = pdest;

	if (pdest && psrc && num)
		p = memmove(pdest, psrc, num);

	return p;
}

/**
 *  @brief Compare two memory areas
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param pmem1    Pointer to the first memory
 *  @param pmem2    Pointer to the second memory
 *  @param num      Number of bytes to compare
 *
 *  @return         Compare result returns by memcmp
 */
t_s32
moal_memcmp(IN t_void *pmoal_handle,
	    IN const t_void *pmem1, IN const t_void *pmem2, IN t_u32 num)
{
	t_s32 result;

	result = memcmp(pmem1, pmem2, num);

	return result;
}

/**
 *  @brief Delay function
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param delay  delay in micro-second
 *
 *  @return       N/A
 */
t_void
moal_udelay(IN t_void *pmoal_handle, IN t_u32 delay)
{
	if (delay >= 1000)
		mdelay(delay / 1000);
	if (delay % 1000)
		udelay(delay % 1000);
}

/**
 *  @brief Retrieves the current system time
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param psec     Pointer to buf for the seconds of system time
 *  @param pusec    Pointer to buf the micro seconds of system time
 *
 *  @return         MLAN_STATUS_SUCCESS
 */
mlan_status
moal_get_system_time(IN t_void *pmoal_handle, OUT t_u32 *psec, OUT t_u32 *pusec)
{
	wifi_timeval t;

	woal_get_monotonic_time(&t);
	*psec = (t_u32)t.time_sec;
	*pusec = (t_u32)t.time_usec;

	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief usleep function
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param usmin        Minimum value for sleep in usecs
 *  @param usmax        Maximum value for sleep in usecs
 *
 *  @return         MLAN_STATUS_SUCCESS
 */
mlan_status
moal_usleep(IN t_void *pmoal_handle, IN t_u64 min, IN t_u64 max)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	usleep_range(min, max);
#else
	msleep(min);
#endif

	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Retrieves the current boot time
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param pnsec     Pointer to buf for the Nanoseconds of boot time
 *
 *  @return         MLAN_STATUS_SUCCESS
 */
mlan_status
moal_get_boot_ktime(IN t_void *pmoal_handle, OUT t_u64 *pnsec)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
	ktime_t time;

	time = ktime_get_with_offset(TK_OFFS_BOOT);
	*pnsec = *(t_u64 *)&(time);
#endif
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Initializes the timer
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param pptimer      Pointer to the timer
 *  @param callback     Pointer to callback function
 *  @param pcontext     Pointer to context
 *
 *  @return             MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_init_timer(IN t_void *pmoal_handle,
		OUT t_void **pptimer,
		IN t_void (*callback) (t_void *pcontext), IN t_void *pcontext)
{
	moal_drv_timer *timer = NULL;
	t_u32 mem_flag = (in_interrupt() || irqs_disabled())?
		GFP_ATOMIC : GFP_KERNEL;

	timer = kmalloc(sizeof(moal_drv_timer), mem_flag);
	if (timer == NULL)
		return MLAN_STATUS_FAILURE;
	woal_initialize_timer(timer, callback, pcontext);
	*pptimer = (t_void *)timer;

	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Free the timer
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param ptimer   Pointer to the timer
 *
 *  @return         MLAN_STATUS_SUCCESS
 */
mlan_status
moal_free_timer(IN t_void *pmoal_handle, IN t_void *ptimer)
{
	moal_drv_timer *timer = (moal_drv_timer *)ptimer;

	if (timer) {
		if ((timer->timer_is_canceled == MFALSE) && timer->time_period) {
			PRINTM(MWARN,
			       "mlan try to free timer without stop timer!\n");
			woal_cancel_timer(timer);
		}
		kfree(timer);
	}

	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Start the timer
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param ptimer       Pointer to the timer
 *  @param periodic     Periodic timer
 *  @param msec         Timer value in milliseconds
 *
 *  @return             MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_start_timer(IN t_void *pmoal_handle,
		 IN t_void *ptimer, IN t_u8 periodic, IN t_u32 msec)
{
	if (!ptimer)
		return MLAN_STATUS_FAILURE;

	((moal_drv_timer *)ptimer)->timer_is_periodic = periodic;
	woal_mod_timer((moal_drv_timer *)ptimer, msec);

	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Stop the timer
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param ptimer   Pointer to the timer
 *
 *  @return         MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_stop_timer(IN t_void *pmoal_handle, IN t_void *ptimer)
{
	if (!ptimer)
		return MLAN_STATUS_FAILURE;
	woal_cancel_timer((moal_drv_timer *)ptimer);

	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Initializes the lock
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param pplock   Pointer to the lock
 *
 *  @return         MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_init_lock(IN t_void *pmoal_handle, OUT t_void **pplock)
{
	moal_handle *handle = (moal_handle *)pmoal_handle;
	moal_lock *mlock = NULL;

	mlock = kmalloc(sizeof(moal_lock), GFP_ATOMIC);
	if (!mlock)
		return MLAN_STATUS_FAILURE;
	spin_lock_init(&mlock->lock);
	*pplock = (t_void *)mlock;

	atomic_inc(&handle->lock_count);

	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Free the lock
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param plock    Lock
 *
 *  @return         MLAN_STATUS_SUCCESS
 */
mlan_status
moal_free_lock(IN t_void *pmoal_handle, IN t_void *plock)
{
	moal_handle *handle = (moal_handle *)pmoal_handle;
	moal_lock *mlock = plock;

	kfree(mlock);
	if (mlock)
		atomic_dec(&handle->lock_count);

	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Request a spin lock
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param plock    Pointer to the lock
 *
 *  @return         MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_spin_lock(IN t_void *pmoal_handle, IN t_void *plock)
{
	moal_lock *mlock = plock;
	unsigned long flags = 0;

	if (mlock) {
		spin_lock_irqsave(&mlock->lock, flags);
		mlock->flags = flags;
		return MLAN_STATUS_SUCCESS;
	} else {
		return MLAN_STATUS_FAILURE;
	}
}

/**
 *  @brief Request a spin_unlock
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param plock    Pointer to the lock
 *
 *  @return         MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_spin_unlock(IN t_void *pmoal_handle, IN t_void *plock)
{
	moal_lock *mlock = (moal_lock *)plock;

	if (mlock) {
		spin_unlock_irqrestore(&mlock->lock, mlock->flags);

		return MLAN_STATUS_SUCCESS;
	} else {
		return MLAN_STATUS_FAILURE;
	}
}

/**
 *  @brief This function reads one block of firmware data from MOAL
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param offset       Offset from where the data will be copied
 *  @param len          Length to be copied
 *  @param pbuf         Buffer where the data will be copied
 *
 *  @return             MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_get_fw_data(IN t_void *pmoal_handle,
		 IN t_u32 offset, IN t_u32 len, OUT t_u8 *pbuf)
{
	moal_handle *handle = (moal_handle *)pmoal_handle;

	if (!pbuf || !len)
		return MLAN_STATUS_FAILURE;

	if (offset + len > handle->firmware->size)
		return MLAN_STATUS_FAILURE;

	memcpy(pbuf, handle->firmware->data + offset, len);

	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function is called when MLAN completes the initialization firmware.
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param status   The status code for mlan_init_fw request
 *  @param phw      pointer to mlan_hw_info
 *  @param ptbl     pointer to mplan_bss_tbl
 *  @return         MLAN_STATUS_SUCCESS
 */
mlan_status
moal_get_hw_spec_complete(IN t_void *pmoal_handle, IN mlan_status status,
			  IN mlan_hw_info * phw, IN pmlan_bss_tbl ptbl)
{
	ENTER();
	if (status == MLAN_STATUS_SUCCESS) {
		PRINTM(MCMND, "Get Hw Spec done, fw_cap=0x%x\n", phw->fw_cap);
	}
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function is called when MLAN completes the initialization firmware.
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param status   The status code for mlan_init_fw request
 *
 *  @return         MLAN_STATUS_SUCCESS
 */
mlan_status
moal_init_fw_complete(IN t_void *pmoal_handle, IN mlan_status status)
{
	moal_handle *handle = (moal_handle *)pmoal_handle;
	ENTER();
	if (status == MLAN_STATUS_SUCCESS)
		handle->hardware_status = HardwareStatusReady;
	handle->init_wait_q_woken = MTRUE;
	wake_up(&handle->init_wait_q);
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function is called when MLAN shutdown firmware is completed.
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param status   The status code for mlan_shutdown request
 *
 *  @return         MLAN_STATUS_SUCCESS
 */
mlan_status
moal_shutdown_fw_complete(IN t_void *pmoal_handle, IN mlan_status status)
{
	moal_handle *handle = (moal_handle *)pmoal_handle;
	ENTER();
	handle->hardware_status = HardwareStatusNotReady;
	handle->init_wait_q_woken = MTRUE;
	wake_up_interruptible(&handle->init_wait_q);
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function is called when an MLAN IOCTL is completed.
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param pioctl_req	pointer to structure mlan_ioctl_req
 *  @param status   The status code for mlan_ioctl request
 *
 *  @return         MLAN_STATUS_SUCCESS
 */
mlan_status
moal_ioctl_complete(IN t_void *pmoal_handle,
		    IN pmlan_ioctl_req pioctl_req, IN mlan_status status)
{
	moal_handle *handle = (moal_handle *)pmoal_handle;
	moal_private *priv = NULL;
	wait_queue *wait;
	unsigned long flags = 0;
	ENTER();

	if (!atomic_read(&handle->ioctl_pending))
		PRINTM(MERROR, "ERR: Unexpected IOCTL completed: %p\n",
		       pioctl_req);
	else
		atomic_dec(&handle->ioctl_pending);
	priv = woal_bss_index_to_priv(handle, pioctl_req->bss_index);
	if (!priv) {
		PRINTM(MERROR,
		       "IOCTL %p complete with NULL priv, bss_index=%d\n",
		       pioctl_req, pioctl_req->bss_index);
		LEAVE();
		return MLAN_STATUS_SUCCESS;
	}

	if (status != MLAN_STATUS_SUCCESS && status != MLAN_STATUS_COMPLETE)
		PRINTM(MERROR,
		       "IOCTL failed: %p id=0x%x, sub_id=0x%x action=%d, status_code=0x%x\n",
		       pioctl_req, pioctl_req->req_id,
		       (*(t_u32 *)pioctl_req->pbuf), (int)pioctl_req->action,
		       pioctl_req->status_code);
	else
		PRINTM(MIOCTL,
		       "IOCTL completed: %p id=0x%x sub_id=0x%x, action=%d,  status=%d, status_code=0x%x\n",
		       pioctl_req, pioctl_req->req_id,
		       (*(t_u32 *)pioctl_req->pbuf), (int)pioctl_req->action,
		       status, pioctl_req->status_code);

	spin_lock_irqsave(&handle->driver_lock, flags);
	wait = (wait_queue *)pioctl_req->reserved_1;
	if (wait) {
		wait->condition = MTRUE;
		wait->status = status;
		if (wait->wait_timeout) {
			wake_up(&wait->wait);
		} else {
			if ((status != MLAN_STATUS_SUCCESS) &&
			    (pioctl_req->status_code ==
			     MLAN_ERROR_CMD_TIMEOUT)) {
				PRINTM(MERROR, "IOCTL: command timeout\n");
			} else {
				wake_up_interruptible(&wait->wait);
			}
		}
		spin_unlock_irqrestore(&handle->driver_lock, flags);
	} else {
		spin_unlock_irqrestore(&handle->driver_lock, flags);
		if ((status == MLAN_STATUS_SUCCESS) &&
		    (pioctl_req->action == MLAN_ACT_GET))
			woal_process_ioctl_resp(priv, pioctl_req);
		kfree(pioctl_req);
	}
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function allocates mlan_buffer.
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param size     allocation size requested
 *  @param pmbuf    pointer to pointer to the allocated buffer
 *
 *  @return         MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_alloc_mlan_buffer(IN t_void *pmoal_handle,
		       IN t_u32 size, OUT pmlan_buffer *pmbuf)
{
	*pmbuf = woal_alloc_mlan_buffer((moal_handle *)pmoal_handle, size);
	if (NULL == *pmbuf)
		return MLAN_STATUS_FAILURE;
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function frees mlan_buffer.
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param pmbuf    pointer to buffer to be freed
 *
 *  @return         MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_free_mlan_buffer(IN t_void *pmoal_handle, IN pmlan_buffer pmbuf)
{
	if (!pmbuf)
		return MLAN_STATUS_FAILURE;
	woal_free_mlan_buffer((moal_handle *)pmoal_handle, pmbuf);
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function is called when MLAN complete send data packet.
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param pmbuf    Pointer to the mlan buffer structure
 *  @param status   The status code for mlan_send_packet request
 *
 *  @return         MLAN_STATUS_SUCCESS
 */
mlan_status
moal_send_packet_complete(IN t_void *pmoal_handle,
			  IN pmlan_buffer pmbuf, IN mlan_status status)
{
	moal_private *priv = NULL;
	moal_handle *handle = (moal_handle *)pmoal_handle;
	struct sk_buff *skb = NULL;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 29)
	t_u32 index = 0;
#endif

	ENTER();
	if (pmbuf && pmbuf->buf_type == MLAN_BUF_TYPE_RAW_DATA) {
		woal_free_mlan_buffer(handle, pmbuf);
		atomic_dec(&handle->tx_pending);
		goto done;
	}
	if (pmbuf) {
		priv = woal_bss_index_to_priv(pmoal_handle, pmbuf->bss_index);
		skb = (struct sk_buff *)pmbuf->pdesc;
		if (priv) {
			woal_set_trans_start(priv->netdev);
			if (skb) {
				if (status == MLAN_STATUS_SUCCESS) {
					priv->stats.tx_packets++;
					priv->stats.tx_bytes += skb->len;
				} else {
					priv->stats.tx_errors++;
				}
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 29)
				index = skb_get_queue_mapping(skb);
				atomic_dec(&handle->tx_pending);
				if (atomic_dec_return
				    (&priv->wmm_tx_pending[index]) ==
				    LOW_TX_PENDING) {
					struct netdev_queue *txq =
						netdev_get_tx_queue(priv->
								    netdev,
								    index);
					if (netif_tx_queue_stopped(txq)) {
						netif_tx_wake_queue(txq);
						PRINTM(MINFO,
						       "Wakeup Kernel Queue:%d\n",
						       index);
					}
				}
#else /*#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,29) */
				if (atomic_dec_return(&handle->tx_pending) <
				    LOW_TX_PENDING) {
					int i;
					for (i = 0; i < handle->priv_num; i++) {
#ifdef STA_SUPPORT
						if ((GET_BSS_ROLE
						     (handle->priv[i]) ==
						     MLAN_BSS_ROLE_STA) &&
						    (handle->priv[i]->
						     media_connected ||
						     priv->
						     is_adhoc_link_sensed)) {
							woal_wake_queue(handle->
									priv
									[i]->
									netdev);
						}
#endif
#ifdef UAP_SUPPORT
						if ((GET_BSS_ROLE
						     (handle->priv[i]) ==
						     MLAN_BSS_ROLE_UAP) &&
						    (handle->priv[i]->
						     media_connected)) {
							woal_wake_queue(handle->
									priv
									[i]->
									netdev);
						}
#endif
					}
				}
#endif /*#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,29) */
			}
		}
		if (skb)
			dev_kfree_skb_any(skb);
	}

done:
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function write a command/data packet to card.
 *         This function blocks the call until it finishes
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param pmbuf    Pointer to the mlan buffer structure
 *  @param port     Port number for sent
 *  @param timeout  Timeout value in milliseconds (if 0 the wait is forever)
 *
 *  @return         MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_write_data_sync(IN t_void *pmoal_handle,
		     IN pmlan_buffer pmbuf, IN t_u32 port, IN t_u32 timeout)
{
	return woal_write_data_sync((moal_handle *)pmoal_handle, pmbuf, port,
				    timeout);
}

/**
 *  @brief This function read data packet/event/command from card.
 *         This function blocks the call until it finish
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param pmbuf    Pointer to the mlan buffer structure
 *  @param port     Port number for read
 *  @param timeout  Timeout value in milliseconds (if 0 the wait is forever)
 *
 *  @return         MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_read_data_sync(IN t_void *pmoal_handle,
		    IN OUT pmlan_buffer pmbuf, IN t_u32 port, IN t_u32 timeout)
{
	return woal_read_data_sync((moal_handle *)pmoal_handle, pmbuf, port,
				   timeout);
}

/**
 *  @brief This function writes data into card register.
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param reg          register offset
 *  @param data         value
 *
 *  @return             MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_write_reg(IN t_void *pmoal_handle, IN t_u32 reg, IN t_u32 data)
{
	return woal_write_reg((moal_handle *)pmoal_handle, reg, data);
}

/**
 *  @brief This function reads data from card register.
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param reg          register offset
 *  @param data         value
 *
 *  @return             MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_read_reg(IN t_void *pmoal_handle, IN t_u32 reg, OUT t_u32 *data)
{
	return woal_read_reg((moal_handle *)pmoal_handle, reg, data);
}

/**
 *  @brief This function uploads the packet to the network stack
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param pmbuf    Pointer to the mlan buffer structure
 *
 *  @return         MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
moal_recv_packet(IN t_void *pmoal_handle, IN pmlan_buffer pmbuf)
{
	mlan_status status = MLAN_STATUS_SUCCESS;
	moal_private *priv = NULL;
	struct sk_buff *skb = NULL;
	moal_handle *handle = (moal_handle *)pmoal_handle;
	ENTER();
	if (pmbuf) {

		priv = woal_bss_index_to_priv(pmoal_handle, pmbuf->bss_index);
		skb = (struct sk_buff *)pmbuf->pdesc;
		if (priv) {
			if (skb) {
				skb_reserve(skb, pmbuf->data_offset);
				if (skb_tailroom(skb) < pmbuf->data_len) {
					PRINTM(MERROR,
					       "skb overflow: tail room=%d, data_len=%d\n",
					       skb_tailroom(skb),
					       pmbuf->data_len);
					status = MLAN_STATUS_FAILURE;
					priv->stats.rx_dropped++;
					goto done;
				}
				skb_put(skb, pmbuf->data_len);
				pmbuf->pdesc = NULL;
				pmbuf->pbuf = NULL;
				pmbuf->data_offset = pmbuf->data_len = 0;
				/* pkt been submit to kernel, no need to free by mlan */
				status = MLAN_STATUS_PENDING;
				atomic_dec(&handle->mbufalloc_count);
			} else {
				PRINTM(MERROR,
				       "%s without skb attach!!! pkt_len=%d flags=0x%x\n",
				       __func__, pmbuf->data_len, pmbuf->flags);
				skb = dev_alloc_skb(pmbuf->data_len +
						    MLAN_NET_IP_ALIGN);
				if (!skb) {
					PRINTM(MERROR, "%s fail to alloc skb\n",
					       __func__);
					status = MLAN_STATUS_FAILURE;
					priv->stats.rx_dropped++;
					goto done;
				}
				skb_reserve(skb, MLAN_NET_IP_ALIGN);
				memcpy(skb->data,
				       (t_u8 *)(pmbuf->pbuf +
						pmbuf->data_offset),
				       pmbuf->data_len);
				skb_put(skb, pmbuf->data_len);

			}
			skb->dev = priv->netdev;
			skb->protocol = eth_type_trans(skb, priv->netdev);
			skb->ip_summed = CHECKSUM_NONE;

			priv->stats.rx_bytes += skb->len;
			priv->stats.rx_packets++;
#ifdef ANDROID_KERNEL
			if (wakelock_timeout) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
				__pm_wakeup_event(&handle->ws,
						  wakelock_timeout);
#else
				wake_lock_timeout(&handle->wake_lock,
						  msecs_to_jiffies
						  (wakelock_timeout));
#endif
			}
#endif
			if (in_interrupt())
				netif_rx(skb);
			else {
				if (atomic_read(&handle->rx_pending) >
				    MAX_RX_PENDING_THRHLD)
					netif_rx(skb);
				else
					netif_rx_ni(skb);
			}
		}
	}
done:
	LEAVE();
	return status;
}

/**
 * @brief   Handle RX MGMT PKT event
 *
 * @param priv          A pointer moal_private structure
 * @param pkt        A pointer to pkt
 * @param len        length of pkt
 *
 * @return          N/A
 */
void
woal_rx_mgmt_pkt_event(moal_private *priv, t_u8 *pkt, t_u16 len)
{
	struct woal_event *evt;
	unsigned long flags;
	moal_handle *handle = priv->phandle;

	evt = kzalloc(sizeof(struct woal_event), GFP_ATOMIC);
	if (evt) {
		evt->priv = priv;
		evt->type = WOAL_EVENT_RX_MGMT_PKT;
		evt->evt.event_len = len;
		memcpy(evt->evt.event_buf, pkt,
		       MIN(evt->evt.event_len, sizeof(evt->evt.event_buf)));
		INIT_LIST_HEAD(&evt->link);
		spin_lock_irqsave(&handle->evt_lock, flags);
		list_add_tail(&evt->link, &handle->evt_queue);
		spin_unlock_irqrestore(&handle->evt_lock, flags);
		queue_work(handle->evt_workqueue, &handle->evt_work);
	}
}

/**
 *  @brief This function handles event receive
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param pmevent  Pointer to the mlan event structure
 *
 *  @return         MLAN_STATUS_SUCCESS
 */
mlan_status
moal_recv_event(IN t_void *pmoal_handle, IN pmlan_event pmevent)
{
#ifdef STA_SUPPORT
	int custom_len = 0;
#ifdef STA_CFG80211
	unsigned long flags;
#endif
#endif
	moal_private *priv = NULL;
#if defined(STA_SUPPORT) || defined(UAP_SUPPORT)
	moal_private *pmpriv = NULL;
#endif
#if defined(STA_WEXT) || defined(UAP_WEXT)
#if defined(STA_SUPPORT) || defined(UAP_WEXT)
#if defined(UAP_SUPPORT) || defined(STA_WEXT)
	union iwreq_data wrqu;
#endif
#endif
#endif
#if defined(SDIO_SUSPEND_RESUME)
	mlan_ds_ps_info pm_info;
#endif

#if defined(UAP_CFG80211) || defined(STA_CFG80211)
	chan_band_info *pchan_info = NULL;
#endif
	t_u8 channel_status;
	moal_private *remain_priv = NULL;

	ENTER();

	if ((pmevent->event_id != MLAN_EVENT_ID_DRV_DEFER_RX_WORK) &&
	    (pmevent->event_id != MLAN_EVENT_ID_DRV_DEFER_HANDLING) &&
	    (pmevent->event_id != MLAN_EVENT_ID_DRV_MGMT_FRAME))
		PRINTM(MEVENT, "event id:0x%x\n", pmevent->event_id);
	if (pmevent->event_id == MLAN_EVENT_ID_FW_DUMP_INFO) {
		woal_store_firmware_dump(pmoal_handle, pmevent);
		goto done;
	}
	if (pmevent->event_id == MLAN_EVENT_ID_STORE_HOST_CMD_RESP) {
		woal_save_host_cmdresp((moal_handle *)pmoal_handle,
				       (mlan_cmdresp_event *) pmevent);
		goto done;
	}
	priv = woal_bss_index_to_priv(pmoal_handle, pmevent->bss_index);
	if (priv == NULL) {
		PRINTM(MERROR, "%s: priv is null\n", __func__);
		goto done;
	}
	if (priv->netdev == NULL) {
		PRINTM(MERROR, "%s: netdev is null\n", __func__);
		goto done;
	}
	switch (pmevent->event_id) {
#ifdef STA_SUPPORT
	case MLAN_EVENT_ID_FW_ADHOC_LINK_SENSED:
		priv->is_adhoc_link_sensed = MTRUE;
		if (!netif_carrier_ok(priv->netdev))
			netif_carrier_on(priv->netdev);
		woal_wake_queue(priv->netdev);
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv,
						   CUS_EVT_ADHOC_LINK_SENSED);
#endif
		woal_broadcast_event(priv, CUS_EVT_ADHOC_LINK_SENSED,
				     strlen(CUS_EVT_ADHOC_LINK_SENSED));
		break;

	case MLAN_EVENT_ID_FW_ADHOC_LINK_LOST:
		woal_stop_queue(priv->netdev);
		if (netif_carrier_ok(priv->netdev))
			netif_carrier_off(priv->netdev);
		priv->is_adhoc_link_sensed = MFALSE;
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv,
						   CUS_EVT_ADHOC_LINK_LOST);
#endif
		woal_broadcast_event(priv, CUS_EVT_ADHOC_LINK_LOST,
				     strlen(CUS_EVT_ADHOC_LINK_LOST));
		break;

	case MLAN_EVENT_ID_DRV_CONNECTED:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext) &&
		    pmevent->event_len == ETH_ALEN) {
			memset(wrqu.ap_addr.sa_data, 0x00, ETH_ALEN);
			memcpy(wrqu.ap_addr.sa_data, pmevent->event_buf,
			       ETH_ALEN);
			wrqu.ap_addr.sa_family = ARPHRD_ETHER;
			wireless_send_event(priv->netdev, SIOCGIWAP, &wrqu,
					    NULL);
		}
#endif
#ifdef STA_CFG80211
		if (IS_STA_CFG80211(cfg80211_wext)) {
			memcpy(priv->cfg_bssid, pmevent->event_buf, ETH_ALEN);
			woal_set_scan_time(priv, ACTIVE_SCAN_CHAN_TIME,
					   PASSIVE_SCAN_CHAN_TIME,
					   MIN_SPECIFIC_SCAN_CHAN_TIME);
		}
#endif
		custom_len = strlen(CUS_EVT_AP_CONNECTED);
		memmove(pmevent->event_buf + custom_len, pmevent->event_buf,
			pmevent->event_len);
		memcpy(pmevent->event_buf, CUS_EVT_AP_CONNECTED, custom_len);
		pmevent->event_len += custom_len;
		woal_broadcast_event(priv, pmevent->event_buf,
				     pmevent->event_len);
		woal_update_dscp_mapping(priv);
		priv->media_connected = MTRUE;
		if (!netif_carrier_ok(priv->netdev))
			netif_carrier_on(priv->netdev);
		woal_wake_queue(priv->netdev);

		break;

	case MLAN_EVENT_ID_DRV_SCAN_REPORT:
		PRINTM(MINFO, "Scan report\n");

		if (priv->report_scan_result) {
			priv->report_scan_result = MFALSE;
#ifdef STA_CFG80211
			if (IS_STA_CFG80211(cfg80211_wext)) {
				if (priv->phandle->scan_request) {
					PRINTM(MINFO,
					       "Reporting scan results\n");
					woal_inform_bss_from_scan_result(priv,
									 NULL,
									 MOAL_NO_WAIT);
					if (!priv->phandle->first_scan_done) {
						priv->phandle->first_scan_done =
							MTRUE;
						woal_set_scan_time(priv,
								   ACTIVE_SCAN_CHAN_TIME,
								   PASSIVE_SCAN_CHAN_TIME,
								   SPECIFIC_SCAN_CHAN_TIME);
					}
					spin_lock_irqsave(&priv->phandle->
							  scan_req_lock, flags);
					if (priv->phandle->scan_request) {
						woal_cfg80211_scan_done(priv->
									phandle->
									scan_request,
									MFALSE);
						priv->phandle->scan_request =
							NULL;
					}
					spin_unlock_irqrestore(&priv->phandle->
							       scan_req_lock,
							       flags);
				}
			}
#endif /* STA_CFG80211 */

#ifdef STA_WEXT
			if (IS_STA_WEXT(cfg80211_wext)) {
				memset(&wrqu, 0, sizeof(union iwreq_data));
				wireless_send_event(priv->netdev, SIOCGIWSCAN,
						    &wrqu, NULL);
			}
#endif
			woal_broadcast_event(priv, (t_u8 *)&pmevent->event_id,
					     sizeof(mlan_event_id));

		}

		if (priv->phandle->scan_pending_on_block == MTRUE) {
			priv->phandle->scan_pending_on_block = MFALSE;
			priv->phandle->scan_priv = NULL;
			MOAL_REL_SEMAPHORE(&priv->phandle->async_sem);
		}
		break;

	case MLAN_EVENT_ID_DRV_OBSS_SCAN_PARAM:
		memmove((pmevent->event_buf + strlen(CUS_EVT_OBSS_SCAN_PARAM) +
			 1), pmevent->event_buf, pmevent->event_len);
		memcpy(pmevent->event_buf, (t_u8 *)CUS_EVT_OBSS_SCAN_PARAM,
		       strlen(CUS_EVT_OBSS_SCAN_PARAM));
		pmevent->event_buf[strlen(CUS_EVT_OBSS_SCAN_PARAM)] = 0;
		woal_broadcast_event(priv, pmevent->event_buf,
				     pmevent->event_len +
				     strlen(CUS_EVT_OBSS_SCAN_PARAM));

#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext)) {
			memset(&wrqu, 0, sizeof(union iwreq_data));
			wrqu.data.pointer = pmevent->event_buf;
			wrqu.data.length =
				pmevent->event_len +
				strlen(CUS_EVT_OBSS_SCAN_PARAM) + 1;
			wireless_send_event(priv->netdev, IWEVCUSTOM, &wrqu,
					    pmevent->event_buf);
		}
#endif
		break;
	case MLAN_EVENT_ID_FW_BW_CHANGED:
		memmove((pmevent->event_buf + strlen(CUS_EVT_BW_CHANGED) + 1),
			pmevent->event_buf, pmevent->event_len);
		memcpy(pmevent->event_buf, (t_u8 *)CUS_EVT_BW_CHANGED,
		       strlen(CUS_EVT_BW_CHANGED));
		pmevent->event_buf[strlen(CUS_EVT_BW_CHANGED)] = 0;
		woal_broadcast_event(priv, pmevent->event_buf,
				     pmevent->event_len +
				     strlen(CUS_EVT_BW_CHANGED));

#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext)) {
			memset(&wrqu, 0, sizeof(union iwreq_data));
			wrqu.data.pointer = pmevent->event_buf;
			wrqu.data.length =
				pmevent->event_len +
				strlen(CUS_EVT_BW_CHANGED) + 1;
			wireless_send_event(priv->netdev, IWEVCUSTOM, &wrqu,
					    pmevent->event_buf);
		}
#endif
		break;

	case MLAN_EVENT_ID_FW_DISCONNECTED:

		woal_send_disconnect_to_system(priv,
					       (t_u16)*pmevent->event_buf);
#ifdef STA_CFG80211
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
		priv->auth_flag = 0;
		priv->host_mlme = MFALSE;
		priv->auth_alg = 0xFFFF;
#endif
#endif
#ifdef STA_WEXT
		/* Reset wireless stats signal info */
		if (IS_STA_WEXT(cfg80211_wext)) {
			priv->w_stats.qual.level = 0;
			priv->w_stats.qual.noise = 0;
		}
#endif
#ifdef REASSOCIATION
		if (priv->reassoc_on == MTRUE) {
			PRINTM(MINFO, "Reassoc: trigger the timer\n");
			priv->reassoc_required = MTRUE;
			priv->phandle->is_reassoc_timer_set = MTRUE;
			woal_mod_timer(&priv->phandle->reassoc_timer,
				       REASSOC_TIMER_DEFAULT);
		} else {
			priv->rate_index = AUTO_RATE;
		}
#endif /* REASSOCIATION */
		break;

	case MLAN_EVENT_ID_FW_MIC_ERR_UNI:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext)) {
#if WIRELESS_EXT >= 18
			woal_send_mic_error_event(priv,
						  MLAN_EVENT_ID_FW_MIC_ERR_UNI);
#else
			woal_send_iwevcustom_event(priv,
						   CUS_EVT_MLME_MIC_ERR_UNI);
#endif
		}
#endif /* STA_WEXT */
#ifdef STA_CFG80211
		if (IS_STA_CFG80211(cfg80211_wext)) {
			cfg80211_michael_mic_failure(priv->netdev,
						     priv->cfg_bssid,
						     NL80211_KEYTYPE_PAIRWISE,
						     -1, NULL, GFP_KERNEL);
		}
#endif
		woal_broadcast_event(priv, CUS_EVT_MLME_MIC_ERR_UNI,
				     strlen(CUS_EVT_MLME_MIC_ERR_UNI));
		break;
	case MLAN_EVENT_ID_FW_MIC_ERR_MUL:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext)) {
#if WIRELESS_EXT >= 18
			woal_send_mic_error_event(priv,
						  MLAN_EVENT_ID_FW_MIC_ERR_MUL);
#else
			woal_send_iwevcustom_event(priv,
						   CUS_EVT_MLME_MIC_ERR_MUL);
#endif
		}
#endif /* STA_WEXT */
#ifdef STA_CFG80211
		if (IS_STA_CFG80211(cfg80211_wext)) {
			cfg80211_michael_mic_failure(priv->netdev,
						     priv->cfg_bssid,
						     NL80211_KEYTYPE_GROUP, -1,
						     NULL, GFP_KERNEL);
		}
#endif
		woal_broadcast_event(priv, CUS_EVT_MLME_MIC_ERR_MUL,
				     strlen(CUS_EVT_MLME_MIC_ERR_MUL));
		break;
	case MLAN_EVENT_ID_FW_BCN_RSSI_LOW:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv,
						   CUS_EVT_BEACON_RSSI_LOW);
#endif
#ifdef STA_CFG80211
		if (IS_STA_CFG80211(cfg80211_wext)) {
#if CFG80211_VERSION_CODE > KERNEL_VERSION(2, 6, 35)
			cfg80211_cqm_rssi_notify(priv->netdev,
						 NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW,
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
						 *(t_s16 *)pmevent->event_buf,
#endif
						 GFP_KERNEL);
			priv->last_event |= EVENT_BCN_RSSI_LOW;
#endif
			if (!hw_test && priv->roaming_enabled)
				woal_config_bgscan_and_rssi(priv, MTRUE);
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			woal_cfg80211_rssi_monitor_event(priv,
							 *(t_s16 *)pmevent->
							 event_buf);
#endif
		}
#endif
		woal_broadcast_event(priv, CUS_EVT_BEACON_RSSI_LOW,
				     strlen(CUS_EVT_BEACON_RSSI_LOW));
		break;
	case MLAN_EVENT_ID_FW_BCN_RSSI_HIGH:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv,
						   CUS_EVT_BEACON_RSSI_HIGH);
#endif
#ifdef STA_CFG80211
		if (IS_STA_CFG80211(cfg80211_wext)) {
			if (!priv->mrvl_rssi_low) {
#if CFG80211_VERSION_CODE > KERNEL_VERSION(2, 6, 35)
				cfg80211_cqm_rssi_notify(priv->netdev,
							 NL80211_CQM_RSSI_THRESHOLD_EVENT_HIGH,
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
							 *(t_s16 *)pmevent->
							 event_buf,
#endif
							 GFP_KERNEL);
#endif
				woal_set_rssi_threshold(priv,
							MLAN_EVENT_ID_FW_BCN_RSSI_HIGH,
							MOAL_NO_WAIT);
			}
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			woal_cfg80211_rssi_monitor_event(priv,
							 *(t_s16 *)pmevent->
							 event_buf);
#endif
		}
#endif
		woal_broadcast_event(priv, CUS_EVT_BEACON_RSSI_HIGH,
				     strlen(CUS_EVT_BEACON_RSSI_HIGH));
		break;
	case MLAN_EVENT_ID_FW_BCN_SNR_LOW:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv,
						   CUS_EVT_BEACON_SNR_LOW);
#endif
		woal_broadcast_event(priv, CUS_EVT_BEACON_SNR_LOW,
				     strlen(CUS_EVT_BEACON_SNR_LOW));
		break;
	case MLAN_EVENT_ID_FW_BCN_SNR_HIGH:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv,
						   CUS_EVT_BEACON_SNR_HIGH);
#endif
		woal_broadcast_event(priv, CUS_EVT_BEACON_SNR_HIGH,
				     strlen(CUS_EVT_BEACON_SNR_HIGH));
		break;
	case MLAN_EVENT_ID_FW_MAX_FAIL:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv, CUS_EVT_MAX_FAIL);
#endif
		woal_broadcast_event(priv, CUS_EVT_MAX_FAIL,
				     strlen(CUS_EVT_MAX_FAIL));
		break;
	case MLAN_EVENT_ID_FW_DATA_RSSI_LOW:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv, CUS_EVT_DATA_RSSI_LOW);
#endif
		woal_broadcast_event(priv, CUS_EVT_DATA_RSSI_LOW,
				     strlen(CUS_EVT_DATA_RSSI_LOW));
		break;
	case MLAN_EVENT_ID_FW_DATA_SNR_LOW:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv, CUS_EVT_DATA_SNR_LOW);
#endif
		woal_broadcast_event(priv, CUS_EVT_DATA_SNR_LOW,
				     strlen(CUS_EVT_DATA_SNR_LOW));
		break;
	case MLAN_EVENT_ID_FW_DATA_RSSI_HIGH:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv,
						   CUS_EVT_DATA_RSSI_HIGH);
#endif
		woal_broadcast_event(priv, CUS_EVT_DATA_RSSI_HIGH,
				     strlen(CUS_EVT_DATA_RSSI_HIGH));
		break;
	case MLAN_EVENT_ID_FW_DATA_SNR_HIGH:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv, CUS_EVT_DATA_SNR_HIGH);
#endif
		woal_broadcast_event(priv, CUS_EVT_DATA_SNR_HIGH,
				     strlen(CUS_EVT_DATA_SNR_HIGH));
		break;
	case MLAN_EVENT_ID_FW_LINK_QUALITY:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv, CUS_EVT_LINK_QUALITY);
#endif
		woal_broadcast_event(priv, CUS_EVT_LINK_QUALITY,
				     strlen(CUS_EVT_LINK_QUALITY));
		break;
	case MLAN_EVENT_ID_FW_PORT_RELEASE:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv, CUS_EVT_PORT_RELEASE);
#endif
		woal_broadcast_event(priv, CUS_EVT_PORT_RELEASE,
				     strlen(CUS_EVT_PORT_RELEASE));
		break;
	case MLAN_EVENT_ID_FW_PRE_BCN_LOST:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv,
						   CUS_EVT_PRE_BEACON_LOST);
#endif
#ifdef STA_CFG80211
#if CFG80211_VERSION_CODE > KERNEL_VERSION(2, 6, 35)
		if (IS_STA_CFG80211(cfg80211_wext)) {
			struct cfg80211_bss *bss = NULL;
			bss = cfg80211_get_bss(priv->wdev->wiphy, NULL,
					       priv->cfg_bssid, NULL, 0,
					       WLAN_CAPABILITY_ESS,
					       WLAN_CAPABILITY_ESS);
			if (bss) {
				cfg80211_unlink_bss(priv->wdev->wiphy, bss);
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)
				cfg80211_put_bss(priv->wdev->wiphy, bss);
#else
				cfg80211_put_bss(bss);
#endif
			}
			if (!hw_test && priv->roaming_enabled)
				woal_config_bgscan_and_rssi(priv, MFALSE);
			priv->last_event |= EVENT_PRE_BCN_LOST;
		}
#endif
#endif
		woal_broadcast_event(priv, CUS_EVT_PRE_BEACON_LOST,
				     strlen(CUS_EVT_PRE_BEACON_LOST));
		break;
	case MLAN_EVENT_ID_FW_DEBUG_INFO:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv, pmevent->event_buf);
#endif
		memmove((pmevent->event_buf + strlen(FW_DEBUG_INFO) + 1),
			pmevent->event_buf, pmevent->event_len);
		memcpy(pmevent->event_buf, (t_u8 *)FW_DEBUG_INFO,
		       strlen(FW_DEBUG_INFO));
		pmevent->event_buf[strlen(FW_DEBUG_INFO)] = 0;
		woal_broadcast_event(priv, pmevent->event_buf,
				     pmevent->event_len +
				     strlen(FW_DEBUG_INFO) + 1);
		break;
	case MLAN_EVENT_ID_FW_WMM_CONFIG_CHANGE:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv,
						   WMM_CONFIG_CHANGE_INDICATION);
#endif
		woal_broadcast_event(priv, WMM_CONFIG_CHANGE_INDICATION,
				     strlen(WMM_CONFIG_CHANGE_INDICATION));
		break;

	case MLAN_EVENT_ID_DRV_REPORT_STRING:
		PRINTM(MINFO, "Report string %s\n", pmevent->event_buf);
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv, pmevent->event_buf);
#endif
		woal_broadcast_event(priv, pmevent->event_buf,
				     strlen(pmevent->event_buf));
		break;
	case MLAN_EVENT_ID_FW_WEP_ICV_ERR:
		DBG_HEXDUMP(MCMD_D, "WEP ICV error", pmevent->event_buf,
			    pmevent->event_len);
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv, CUS_EVT_WEP_ICV_ERR);
#endif
		woal_broadcast_event(priv, CUS_EVT_WEP_ICV_ERR,
				     strlen(CUS_EVT_WEP_ICV_ERR));
		break;

	case MLAN_EVENT_ID_DRV_DEFER_HANDLING:
		queue_work(priv->phandle->workqueue, &priv->phandle->main_work);
		break;
	case MLAN_EVENT_ID_DRV_FLUSH_RX_WORK:
		if (napi) {
			napi_synchronize(&priv->phandle->napi_rx);
			break;
		}
		flush_workqueue(priv->phandle->rx_workqueue);
		break;
	case MLAN_EVENT_ID_DRV_FLUSH_MAIN_WORK:
		flush_workqueue(priv->phandle->workqueue);
		break;
	case MLAN_EVENT_ID_DRV_DEFER_RX_WORK:
		if (napi) {
			napi_schedule(&priv->phandle->napi_rx);
			break;
		}
		queue_work(priv->phandle->rx_workqueue,
			   &priv->phandle->rx_work);
		break;
	case MLAN_EVENT_ID_DRV_DBG_DUMP:
		priv->phandle->driver_status = MTRUE;
		woal_moal_debug_info(priv, NULL, MFALSE);
		woal_broadcast_event(priv, CUS_EVT_DRIVER_HANG,
				     strlen(CUS_EVT_DRIVER_HANG));
#ifdef STA_CFG80211
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
		if (IS_STA_OR_UAP_CFG80211(cfg80211_wext))
			woal_cfg80211_vendor_event(priv, event_hang,
						   CUS_EVT_DRIVER_HANG,
						   strlen(CUS_EVT_DRIVER_HANG));
#endif
#endif
		woal_process_hang(priv->phandle);
		break;
	case MLAN_EVENT_ID_FW_BG_SCAN:
		if (priv->media_connected == MTRUE)
			priv->bg_scan_start = MFALSE;
		priv->bg_scan_reported = MTRUE;
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext)) {
			memset(&wrqu, 0, sizeof(union iwreq_data));
			wireless_send_event(priv->netdev, SIOCGIWSCAN, &wrqu,
					    NULL);
		}
#endif
#ifdef STA_CFG80211
		if (IS_STA_CFG80211(cfg80211_wext)) {
			priv->last_event |= EVENT_BG_SCAN_REPORT;
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
			if (priv->sched_scanning &&
			    !priv->phandle->cfg80211_suspend) {
				mlan_scan_resp scan_resp;
				if (MLAN_STATUS_SUCCESS ==
				    woal_get_scan_table(priv, MOAL_NO_WAIT,
							&scan_resp))
					PRINTM(MIOCTL,
					       "Triggered mlan get bgscan result\n");
			}
#endif
			if (!hw_test && priv->roaming_enabled
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
			    && !priv->phandle->cfg80211_suspend
#endif
				) {
				priv->roaming_required = MTRUE;
#ifdef ANDROID_KERNEL
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
				__pm_wakeup_event(&priv->phandle->ws,
						  ROAMING_WAKE_LOCK_TIMEOUT);
#else
				wake_lock_timeout(&priv->phandle->wake_lock,
						  msecs_to_jiffies
						  (ROAMING_WAKE_LOCK_TIMEOUT));
#endif
#endif
				wake_up_interruptible(&priv->phandle->
						      reassoc_thread.wait_q);
			}
		}
#endif
		break;
	case MLAN_EVENT_ID_FW_BG_SCAN_STOPPED:
#ifdef STA_CFG80211
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
		if (IS_STA_CFG80211(cfg80211_wext)) {
			if (priv->sched_scanning) {
#if CFG80211_VERSION_CODE < KERNEL_VERSION(3, 14, 6)
				priv->phandle->rx_bgscan_stop = MTRUE;
				priv->phandle->bg_scan_priv = priv;
				queue_work(priv->phandle->rx_workqueue,
					   &priv->phandle->rx_work);
#else
				if (rtnl_is_locked())
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
					cfg80211_sched_scan_stopped_locked(
#else
					cfg80211_sched_scan_stopped_rtnl(
#endif
									 priv->
									 wdev->
									 wiphy
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
									 , 0
#endif
						);
				else
					cfg80211_sched_scan_stopped(priv->wdev->
								    wiphy
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
								    , 0
#endif
						);
				priv->sched_scanning = MFALSE;
#endif
				PRINTM(MEVENT, "Sched_Scan stopped\n");

			}
		}
#endif
#endif
		break;
	case MLAN_EVENT_ID_DRV_BGSCAN_RESULT:
#ifdef STA_CFG80211
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
		if (IS_STA_CFG80211(cfg80211_wext)) {
			if (priv->sched_scanning &&
			    !priv->phandle->cfg80211_suspend) {
				woal_inform_bss_from_scan_result(priv, NULL,
								 MOAL_NO_WAIT);
				cfg80211_sched_scan_results(priv->wdev->wiphy
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
							    ,
							    priv->bg_scan_reqid
#endif
					);
				priv->last_event = 0;
				PRINTM(MEVENT,
				       "Reporting Sched_Scan results\n");
#if CFG80211_VERSION_CODE < KERNEL_VERSION(3, 14, 6)
				priv->phandle->rx_bgscan_stop = MTRUE;
				priv->phandle->bg_scan_priv = priv;
				queue_work(priv->phandle->rx_workqueue,
					   &priv->phandle->rx_work);
#else
				if (rtnl_is_locked())
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
					cfg80211_sched_scan_stopped_locked(
#else
					cfg80211_sched_scan_stopped_rtnl(
#endif
									 priv->
									 wdev->
									 wiphy
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
									 , 0
#endif
						);
				else
					cfg80211_sched_scan_stopped(priv->wdev->
								    wiphy
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
								    , 0
#endif
						);
				priv->sched_scanning = MFALSE;
#endif
				PRINTM(MEVENT, "Sched_Scan stopped\n");
			}
		}
#endif
#endif
		break;
#ifdef UAP_CFG80211
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)
	case MLAN_EVENT_ID_FW_CHANNEL_REPORT_RDY:
		if (priv->phandle->is_cac_timer_set) {
			t_u8 radar_detected = pmevent->event_buf[0];
			PRINTM(MEVENT, "%s radar found when CAC \n",
			       radar_detected ? "" : "No");
			moal_stop_timer(priv->phandle,
					&priv->phandle->cac_timer);
			priv->phandle->is_cac_timer_set = MFALSE;
			if (radar_detected) {
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
				cfg80211_cac_event(priv->netdev,
						   &priv->phandle->dfs_channel,
						   NL80211_RADAR_CAC_ABORTED,
						   GFP_KERNEL);
#else
				cfg80211_cac_event(priv->netdev,
						   NL80211_RADAR_CAC_ABORTED,
						   GFP_KERNEL);
#endif
				cfg80211_radar_event(priv->wdev->wiphy,
						     &priv->phandle->
						     dfs_channel, GFP_KERNEL);
			} else {
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
				cfg80211_cac_event(priv->netdev,
						   &priv->phandle->dfs_channel,
						   NL80211_RADAR_CAC_FINISHED,
						   GFP_KERNEL);
#else
				cfg80211_cac_event(priv->netdev,
						   NL80211_RADAR_CAC_FINISHED,
						   GFP_KERNEL);
#endif
			}
			memset(&priv->phandle->dfs_channel, 0,
			       sizeof(struct cfg80211_chan_def));
			priv->phandle->cac_bss_index = 0xff;
		}
		break;
	case MLAN_EVENT_ID_FW_RADAR_DETECTED:
		if (priv->phandle->is_cac_timer_set) {
			if (priv->bss_index == priv->phandle->cac_bss_index) {
				PRINTM(MEVENT, "radar detected during CAC \n");
				woal_cancel_timer(&priv->phandle->cac_timer);
				priv->phandle->is_cac_timer_set = MFALSE;
				/* downstream: cancel the unfinished CAC in Firmware */
				woal_11h_cancel_chan_report_ioctl(priv,
								  MOAL_NO_WAIT);
				/* upstream: inform cfg80211 */
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
				cfg80211_cac_event(priv->netdev,
						   &priv->phandle->dfs_channel,
						   NL80211_RADAR_CAC_ABORTED,
						   GFP_KERNEL);
#else
				cfg80211_cac_event(priv->netdev,
						   NL80211_RADAR_CAC_ABORTED,
						   GFP_KERNEL);
#endif
				cfg80211_radar_event(priv->wdev->wiphy,
						     &priv->phandle->
						     dfs_channel, GFP_KERNEL);

				memset(&priv->phandle->dfs_channel, 0,
				       sizeof(priv->phandle->dfs_channel));
				priv->phandle->cac_bss_index = 0xff;
			} else {
				PRINTM(MERROR,
				       " Radar event for incorrect inferface \n");
			}
		} else {
			PRINTM(MEVENT, "radar detected during BSS active \n");
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			if (dfs_offload)
				woal_cfg80211_dfs_vendor_event(priv,
							       event_dfs_radar_detected,
							       &priv->chan);
			else
#endif
				cfg80211_radar_event(priv->wdev->wiphy,
						     &priv->chan, GFP_KERNEL);
		}
		break;
#endif
#endif
	case MLAN_EVENT_ID_FW_CHANNEL_SWITCH_ANN:
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext))
			woal_send_iwevcustom_event(priv,
						   CUS_EVT_CHANNEL_SWITCH_ANN);
#endif
		woal_broadcast_event(priv, CUS_EVT_CHANNEL_SWITCH_ANN,
				     strlen(CUS_EVT_CHANNEL_SWITCH_ANN));
		break;
#endif /* STA_SUPPORT */
	case MLAN_EVENT_ID_FW_CHAN_SWITCH_COMPLETE:
#if defined(UAP_CFG80211) || defined(STA_CFG80211)
		pchan_info = (chan_band_info *) pmevent->event_buf;
		if (IS_STA_OR_UAP_CFG80211(cfg80211_wext)) {
			PRINTM(MMSG,
			       "CSA/ECSA: Switch to new channel %d complete!\n",
			       pchan_info->channel);
			priv->channel = pchan_info->channel;
#ifdef UAP_CFG80211
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3,12,0)
			if (priv->csa_chan.chan &&
			    (pchan_info->channel ==
			     priv->csa_chan.chan->hw_value)) {
				memcpy(&priv->chan, &priv->csa_chan,
				       sizeof(struct cfg80211_chan_def));
			}
#endif
#endif
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3,8,0)
			if (MFALSE
#ifdef UAP_CFG80211
			    || priv->uap_host_based
#endif
#ifdef STA_CFG80211
			    || priv->sme_current.ssid_len
#endif
				) {
				PRINTM(MEVENT,
				       "CHAN_SWITCH: 11n=%d, chan=%d, center_chan=%d, band=%d, width=%d, 2Offset=%d\n",
				       pchan_info->is_11n_enabled,
				       pchan_info->channel,
				       pchan_info->center_chan,
				       pchan_info->bandcfg.chanBand,
				       pchan_info->bandcfg.chanWidth,
				       pchan_info->bandcfg.chan2Offset);
				woal_channel_switch_event(priv, pchan_info);
			}
#endif
		}
#endif
#ifdef UAP_SUPPORT
		if (priv->bss_role == MLAN_BSS_ROLE_UAP) {
			if (priv->uap_tx_blocked) {
				if (!netif_carrier_ok(priv->netdev))
					netif_carrier_on(priv->netdev);
				woal_start_queue(priv->netdev);
				priv->uap_tx_blocked = MFALSE;
			}
			priv->phandle->chsw_wait_q_woken = MTRUE;
			wake_up_interruptible(&priv->phandle->chsw_wait_q);
		}
#endif
		break;
	case MLAN_EVENT_ID_FW_STOP_TX:
		woal_stop_queue(priv->netdev);
		if (netif_carrier_ok(priv->netdev))
			netif_carrier_off(priv->netdev);
		break;
	case MLAN_EVENT_ID_FW_START_TX:
		if (!netif_carrier_ok(priv->netdev))
			netif_carrier_on(priv->netdev);
		woal_wake_queue(priv->netdev);
		break;
	case MLAN_EVENT_ID_FW_HS_WAKEUP:
		/* simulate HSCFG_CANCEL command */
		woal_cancel_hs(priv, MOAL_NO_WAIT);
#ifdef STA_SUPPORT
		pmpriv = woal_get_priv((moal_handle *)pmoal_handle,
				       MLAN_BSS_ROLE_STA);
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext) && pmpriv)
			woal_send_iwevcustom_event(pmpriv, CUS_EVT_HS_WAKEUP);
#endif /* STA_WEXT */
		if (pmpriv)
			woal_broadcast_event(pmpriv, CUS_EVT_HS_WAKEUP,
					     strlen(CUS_EVT_HS_WAKEUP));
#endif /*STA_SUPPORT */
#ifdef UAP_SUPPORT
		pmpriv = woal_get_priv((moal_handle *)pmoal_handle,
				       MLAN_BSS_ROLE_UAP);
		if (pmpriv) {
			pmevent->event_id = UAP_EVENT_ID_HS_WAKEUP;
			woal_broadcast_event(pmpriv, (t_u8 *)&pmevent->event_id,
					     sizeof(t_u32));
		}
#endif /* UAP_SUPPORT */
		break;
	case MLAN_EVENT_ID_DRV_HS_ACTIVATED:
#ifdef STA_SUPPORT
		pmpriv = woal_get_priv((moal_handle *)pmoal_handle,
				       MLAN_BSS_ROLE_STA);
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext) && pmpriv)
			woal_send_iwevcustom_event(pmpriv,
						   CUS_EVT_HS_ACTIVATED);
#endif /* STA_WEXT */
		if (pmpriv)
			woal_broadcast_event(pmpriv, CUS_EVT_HS_ACTIVATED,
					     strlen(CUS_EVT_HS_ACTIVATED));
#endif /* STA_SUPPORT */
#if defined(UAP_SUPPORT)
		pmpriv = woal_get_priv((moal_handle *)pmoal_handle,
				       MLAN_BSS_ROLE_UAP);
		if (pmpriv) {
			pmevent->event_id = UAP_EVENT_ID_DRV_HS_ACTIVATED;
			woal_broadcast_event(pmpriv, (t_u8 *)&pmevent->event_id,
					     sizeof(t_u32));
		}
#endif
#if defined(SDIO_SUSPEND_RESUME)
		if (priv->phandle->suspend_fail == MFALSE) {
			woal_get_pm_info(priv, &pm_info);
			if (pm_info.is_suspend_allowed == MTRUE) {
				priv->phandle->hs_activated = MTRUE;
#ifdef MMC_PM_FUNC_SUSPENDED
				woal_wlan_is_suspended(priv->phandle);
#endif
			}
			priv->phandle->hs_activate_wait_q_woken = MTRUE;
			wake_up(&priv->phandle->hs_activate_wait_q);
		}
#endif
		break;
	case MLAN_EVENT_ID_DRV_HS_DEACTIVATED:
#ifdef STA_SUPPORT
		pmpriv = woal_get_priv((moal_handle *)pmoal_handle,
				       MLAN_BSS_ROLE_STA);
#ifdef STA_WEXT
		if (IS_STA_WEXT(cfg80211_wext) && pmpriv)
			woal_send_iwevcustom_event(pmpriv,
						   CUS_EVT_HS_DEACTIVATED);
#endif /* STA_WEXT */
		if (pmpriv)
			woal_broadcast_event(pmpriv, CUS_EVT_HS_DEACTIVATED,
					     strlen(CUS_EVT_HS_DEACTIVATED));
#endif /* STA_SUPPORT */
#if defined(UAP_SUPPORT)
		pmpriv = woal_get_priv((moal_handle *)pmoal_handle,
				       MLAN_BSS_ROLE_UAP);
		if (pmpriv) {
			pmevent->event_id = UAP_EVENT_ID_DRV_HS_DEACTIVATED;
			woal_broadcast_event(pmpriv, (t_u8 *)&pmevent->event_id,
					     sizeof(t_u32));
		}
#endif
#if defined(SDIO_SUSPEND_RESUME)
		priv->phandle->hs_activated = MFALSE;
#endif
		break;
#ifdef UAP_SUPPORT
	case MLAN_EVENT_ID_UAP_FW_BSS_START:
		woal_hist_data_reset(priv);
		priv->bss_started = MTRUE;
		if (!netif_carrier_ok(priv->netdev))
			netif_carrier_on(priv->netdev);
		woal_start_queue(priv->netdev);
		memcpy(priv->current_addr, pmevent->event_buf + 6, ETH_ALEN);
		memcpy(priv->netdev->dev_addr, priv->current_addr, ETH_ALEN);
		woal_broadcast_event(priv, pmevent->event_buf,
				     pmevent->event_len);
#ifdef STA_SUPPORT
#ifdef STA_CFG80211
		pmpriv = woal_get_priv((moal_handle *)pmoal_handle,
				       MLAN_BSS_ROLE_STA);
		if (IS_STA_CFG80211(cfg80211_wext) && pmpriv)
			woal_set_scan_time(pmpriv, ACTIVE_SCAN_CHAN_TIME,
					   PASSIVE_SCAN_CHAN_TIME,
					   MIN_SPECIFIC_SCAN_CHAN_TIME);
#endif
#endif
		break;
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
	case MLAN_EVENT_ID_DRV_UAP_CHAN_INFO:
#ifdef UAP_CFG80211
		if (IS_UAP_CFG80211(cfg80211_wext)) {
			chan_band_info *pchan_info =
				(chan_band_info *) pmevent->event_buf;
			PRINTM(MEVENT,
			       "UAP: 11n=%d, chan=%d, center_chan=%d, band=%d, width=%d, 2Offset=%d\n",
			       pchan_info->is_11n_enabled, pchan_info->channel,
			       pchan_info->center_chan,
			       pchan_info->bandcfg.chanBand,
			       pchan_info->bandcfg.chanWidth,
			       pchan_info->bandcfg.chan2Offset);
			if (priv->uap_host_based &&
			    ((priv->channel != pchan_info->channel)
#if defined(DFS_TESTING_SUPPORT)
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)
			     || (priv->chan_under_nop)
#endif
#endif
			    ))
				woal_channel_switch_event(priv, pchan_info);
#if defined(DFS_TESTING_SUPPORT)
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)
			priv->chan_under_nop = MFALSE;
#endif
#endif
		}
#endif
		break;
#endif
	case MLAN_EVENT_ID_UAP_FW_BSS_ACTIVE:
		priv->media_connected = MTRUE;
		if (!netif_carrier_ok(priv->netdev))
			netif_carrier_on(priv->netdev);
		woal_wake_queue(priv->netdev);
		woal_broadcast_event(priv, pmevent->event_buf,
				     pmevent->event_len);
		break;
	case MLAN_EVENT_ID_UAP_FW_BSS_IDLE:
		priv->media_connected = MFALSE;
		woal_broadcast_event(priv, pmevent->event_buf,
				     pmevent->event_len);
		break;
#if defined(STA_CFG80211) || defined(UAP_CFG80211)
	case MLAN_EVENT_ID_FW_REMAIN_ON_CHAN_EXPIRED:
		if (IS_STA_OR_UAP_CFG80211(cfg80211_wext)) {
			PRINTM(MEVENT,
			       "FW_REMAIN_ON_CHANNEL_EXPIRED cookie = %#llx\n",
			       priv->phandle->cookie);
			if (priv->host_mlme &&
			    (priv->auth_flag & HOST_MLME_AUTH_PENDING)) {
				priv->auth_flag = 0;
				priv->host_mlme = MFALSE;
				priv->auth_alg = 0xFFFF;
			}
			priv->phandle->remain_on_channel = MFALSE;
			if (priv->phandle->cookie &&
			    !priv->phandle->is_remain_timer_set) {
				cfg80211_remain_on_channel_expired(
#if CFG80211_VERSION_CODE < KERNEL_VERSION(3, 6, 0)
									  priv->
									  netdev,
#else
									  priv->
									  wdev,
#endif
									  priv->
									  phandle->
									  cookie,
									  &priv->
									  phandle->
									  chan,
#if CFG80211_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
									  priv->
									  phandle->
									  channel_type,
#endif
									  GFP_ATOMIC);
				priv->phandle->cookie = 0;
			}
		}
		break;
#endif
	case MLAN_EVENT_ID_UAP_FW_STA_CONNECT:
#if defined(STA_CFG80211) || defined(UAP_CFG80211)
		if (IS_STA_OR_UAP_CFG80211(cfg80211_wext)) {
			struct station_info sinfo = { 0 };
			t_u8 addr[ETH_ALEN];

			sinfo.filled = 0;
			sinfo.generation = 0;
			/* copy the station mac address */
			memset(addr, 0xFF, ETH_ALEN);
			memcpy(addr, pmevent->event_buf, ETH_ALEN);
		/** these field add in kernel 3.2, but some
				 * kernel do have the pacth to support it,
				 * like T3T and pxa978T 3.0.31 JB, these
				 * patch are needed to support
				 * wpa_supplicant 2.x */
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 0, 31)
			if (pmevent->event_len > ETH_ALEN) {
#if CFG80211_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
				/* set station info filled flag */
				sinfo.filled |= STATION_INFO_ASSOC_REQ_IES;
#endif
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(4, 18, 0)
				sinfo.pertid = NULL;
#endif
				/* get the assoc request ies and length */
				sinfo.assoc_req_ies =
					(const t_u8 *)(pmevent->event_buf +
						       ETH_ALEN);
				sinfo.assoc_req_ies_len =
					pmevent->event_len - ETH_ALEN;

			}
#endif /* KERNEL_VERSION */
			if (priv->netdev && priv->wdev)
				cfg80211_new_sta(priv->netdev,
						 (t_u8 *)addr, &sinfo,
						 GFP_KERNEL);
		}
#endif /* UAP_CFG80211 */
		memmove((pmevent->event_buf + strlen(CUS_EVT_STA_CONNECTED) +
			 1), pmevent->event_buf, pmevent->event_len);
		memcpy(pmevent->event_buf, (t_u8 *)CUS_EVT_STA_CONNECTED,
		       strlen(CUS_EVT_STA_CONNECTED));
		pmevent->event_buf[strlen(CUS_EVT_STA_CONNECTED)] = 0;
		woal_broadcast_event(priv, pmevent->event_buf,
				     pmevent->event_len +
				     strlen(CUS_EVT_STA_CONNECTED));
#ifdef UAP_WEXT
		if (IS_UAP_WEXT(cfg80211_wext)) {
			memset(&wrqu, 0, sizeof(union iwreq_data));
			wrqu.data.pointer = pmevent->event_buf;
			if ((pmevent->event_len +
			     strlen(CUS_EVT_STA_CONNECTED) + 1) > IW_CUSTOM_MAX)
				wrqu.data.length =
					ETH_ALEN +
					strlen(CUS_EVT_STA_CONNECTED) + 1;
			else
				wrqu.data.length =
					pmevent->event_len +
					strlen(CUS_EVT_STA_CONNECTED) + 1;
			wireless_send_event(priv->netdev, IWEVCUSTOM, &wrqu,
					    pmevent->event_buf);
		}
#endif /* UAP_WEXT */
		break;
	case MLAN_EVENT_ID_UAP_FW_STA_DISCONNECT:
#ifdef UAP_CFG80211
		if (IS_UAP_CFG80211(cfg80211_wext)) {
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
			/* skip 2 bytes extra header will get the mac address */
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
			/**Forward Deauth, Auth and disassoc frame to Host*/
			if (host_mlme && priv->uap_host_based) {
				t_u16 reason_code =
					woal_le16_to_cpu(*(t_u16 *)pmevent->
							 event_buf);
				PRINTM(MCMND, "deauth reason code =0x%x\n",
				       reason_code);
				/** BIT 14 indicate deauth is initiated by FW */
				if (reason_code & MBIT(14))
					woal_host_mlme_disconnect(priv, 0,
								  pmevent->
								  event_buf +
								  2);
			} else
#endif
			if (priv->netdev && priv->wdev)
				cfg80211_del_sta(priv->netdev,
						 pmevent->event_buf + 2,
						 GFP_KERNEL);
#endif /* KERNEL_VERSION */
		}
#endif /* UAP_CFG80211 */
		memmove((pmevent->event_buf + strlen(CUS_EVT_STA_DISCONNECTED) +
			 1), pmevent->event_buf, pmevent->event_len);
		memcpy(pmevent->event_buf, (t_u8 *)CUS_EVT_STA_DISCONNECTED,
		       strlen(CUS_EVT_STA_DISCONNECTED));
		pmevent->event_buf[strlen(CUS_EVT_STA_DISCONNECTED)] = 0;
		woal_broadcast_event(priv, pmevent->event_buf,
				     pmevent->event_len +
				     strlen(CUS_EVT_STA_DISCONNECTED));

#ifdef UAP_WEXT
		if (IS_UAP_WEXT(cfg80211_wext)) {
			memset(&wrqu, 0, sizeof(union iwreq_data));
			wrqu.data.pointer = pmevent->event_buf;
			wrqu.data.length =
				pmevent->event_len +
				strlen(CUS_EVT_STA_DISCONNECTED) + 1;
			wireless_send_event(priv->netdev, IWEVCUSTOM, &wrqu,
					    pmevent->event_buf);
		}
#endif /* UAP_WEXT */
		break;
	case MLAN_EVENT_ID_DRV_MGMT_FRAME:
#ifdef UAP_WEXT
		if (IS_UAP_WEXT(cfg80211_wext)) {
			woal_broadcast_event(priv, pmevent->event_buf,
					     pmevent->event_len);
		}
#endif /* UAP_WEXT */
#if defined(STA_CFG80211) || defined(UAP_CFG80211)
		if (IS_STA_OR_UAP_CFG80211(cfg80211_wext)) {
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
			if (priv->netdev
			    && priv->netdev->ieee80211_ptr->wiphy->mgmt_stypes
			    && priv->mgmt_subtype_mask) {
				/* frmctl + durationid + addr1 + addr2 + addr3 + seqctl */
#define PACKET_ADDR4_POS        (2 + 2 + 6 + 6 + 6 + 2)
				t_u8 *pkt;
				int freq =
					priv->phandle->
					remain_on_channel ? priv->phandle->chan.
					center_freq :
					woal_get_active_intf_freq(priv);
				if (!freq) {
					if (!priv->phandle->chan.center_freq) {
						PRINTM(MINFO,
						       "Skip to report mgmt packet to cfg80211\n");
						break;
					}
					freq = priv->phandle->chan.center_freq;
				}

				pkt = ((t_u8 *)pmevent->event_buf
				       + sizeof(pmevent->event_id));

				/* move addr4 */
				memmove(pkt + PACKET_ADDR4_POS,
					pkt + PACKET_ADDR4_POS + ETH_ALEN,
					pmevent->event_len -
					sizeof(pmevent->event_id)
					- PACKET_ADDR4_POS - ETH_ALEN);
#ifdef WIFI_DIRECT_SUPPORT
				if (ieee80211_is_action
				    (((struct ieee80211_mgmt *)pkt)->
				     frame_control))
					woal_cfg80211_display_p2p_actframe(pkt,
									   pmevent->
									   event_len
									   -
									   sizeof
									   (pmevent->
									    event_id)
									   -
									   MLAN_MAC_ADDR_LENGTH,
									   ieee80211_get_channel
									   (priv->
									    wdev->
									    wiphy,
									    freq),
									   MFALSE);
#endif
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
		    /**Forward Deauth, Auth and disassoc frame to Host*/
				if (priv->host_mlme &&
				    (GET_BSS_ROLE(priv) != MLAN_BSS_ROLE_UAP) &&
				    (ieee80211_is_deauth
				     (((struct ieee80211_mgmt *)pkt)->
				      frame_control)
				     ||
				     ieee80211_is_auth(((struct ieee80211_mgmt
							 *)pkt)->frame_control)
				     ||
				     ieee80211_is_disassoc(((struct
							     ieee80211_mgmt *)
							    pkt)->
							   frame_control))) {

					if (ieee80211_is_auth
					    (((struct ieee80211_mgmt *)pkt)->
					     frame_control)) {
						PRINTM(MEVENT,
						       "HostMlme %s: Received auth frame type = 0x%x\n",
						       priv->netdev->name,
						       priv->auth_alg);

						if (priv->
						    auth_flag &
						    HOST_MLME_AUTH_PENDING) {
							if (priv->auth_alg !=
							    WLAN_AUTH_SAE) {
								priv->auth_flag
									&=
									~HOST_MLME_AUTH_PENDING;
								priv->auth_flag
									|=
									HOST_MLME_AUTH_DONE;
								priv->phandle->
									host_mlme_priv
									= priv;
								queue_work
									(priv->
									 phandle->
									 evt_workqueue,
									 &priv->
									 phandle->
									 host_mlme_work);
							}
						} else {
							PRINTM(MERROR,
							       "HostMlme %s: Drop auth frame, auth_flag=0x%x auth_alg=0x%x\n",
							       priv->netdev->
							       name,
							       priv->auth_flag,
							       priv->auth_alg);
							break;
						}
					} else {
						PRINTM(MEVENT,
						       "HostMlme %s: Receive deauth/disassociate\n",
						       priv->netdev->name);
						priv->cfg_disconnect = MTRUE;
						woal_mgmt_frame_register(priv,
									 IEEE80211_STYPE_DEAUTH,
									 MFALSE);
						woal_mgmt_frame_register(priv,
									 IEEE80211_STYPE_DISASSOC,
									 MFALSE);
						woal_send_disconnect_to_system
							(priv,
							 DEF_DEAUTH_REASON_CODE);
						priv->host_mlme = MFALSE;
						priv->auth_flag = 0;
						priv->auth_alg = 0xFFFF;
						if (!priv->wdev->current_bss) {
							PRINTM(MEVENT,
							       "HostMlme: Drop deauth/disassociate, we already disconnected\n");
							break;
						}
					}
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
					woal_rx_mgmt_pkt_event(priv, pkt,
							       pmevent->
							       event_len -
							       sizeof(pmevent->
								      event_id)
							       -
							       MLAN_MAC_ADDR_LENGTH);
#else
					if (ieee80211_is_deauth
					    (((struct ieee80211_mgmt *)pkt)->
					     frame_control))
						cfg80211_send_deauth(priv->
								     netdev,
								     pkt,
								     pmevent->
								     event_len -
								     sizeof
								     (pmevent->
								      event_id)
								     -
								     MLAN_MAC_ADDR_LENGTH);
					else if (ieee80211_is_auth
						 (((struct ieee80211_mgmt *)
						   pkt)->frame_control))
						cfg80211_send_rx_auth(priv->
								      netdev,
								      pkt,
								      pmevent->
								      event_len
								      -
								      sizeof
								      (pmevent->
								       event_id)
								      -
								      MLAN_MAC_ADDR_LENGTH);
					else if (ieee80211_is_disassoc
						 (((struct ieee80211_mgmt *)
						   pkt)->frame_control))
						cfg80211_send_disassoc(priv->
								       netdev,
								       pkt,
								       pmevent->
								       event_len
								       -
								       sizeof
								       (pmevent->
									event_id)
								       -
								       MLAN_MAC_ADDR_LENGTH);

#endif

				} else
#endif
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
					cfg80211_rx_mgmt(
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
								priv->wdev,
#else
								priv->netdev,
#endif
								freq, 0,
								((const t_u8 *)
								 pmevent->
								 event_buf) +
								sizeof(pmevent->
								       event_id),
								pmevent->
								event_len -
								sizeof(pmevent->
								       event_id)
								-
								MLAN_MAC_ADDR_LENGTH
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)
								, 0
#endif
#if CFG80211_VERSION_CODE < KERNEL_VERSION(3, 18, 0)
								, GFP_ATOMIC
#endif
						);
#else
					cfg80211_rx_mgmt(priv->netdev, freq,
							 ((const t_u8 *)
							  pmevent->event_buf) +
							 sizeof(pmevent->
								event_id),
							 pmevent->event_len -
							 sizeof(pmevent->
								event_id) -
							 MLAN_MAC_ADDR_LENGTH,
							 GFP_ATOMIC);
#endif
			}
#endif /* KERNEL_VERSION */
		}
#endif /* STA_CFG80211 || UAP_CFG80211 */
		break;
#endif /* UAP_SUPPORT */
	case MLAN_EVENT_ID_DRV_PASSTHRU:
		woal_broadcast_event(priv, pmevent->event_buf,
				     pmevent->event_len);
		break;
	case MLAN_EVENT_ID_DRV_ASSOC_FAILURE_REPORT:
		PRINTM(MINFO, "Assoc result\n");

		if (priv->media_connected) {
			PRINTM(MINFO, "Assoc_Rpt: Media Connected\n");
			if (!netif_carrier_ok(priv->netdev)) {
				PRINTM(MINFO, "Assoc_Rpt: Carrier On\n");
				netif_carrier_on(priv->netdev);
			}
			PRINTM(MINFO, "Assoc_Rpt: Queue Start\n");
			woal_wake_queue(priv->netdev);
		}
		break;
	case MLAN_EVENT_ID_DRV_MEAS_REPORT:
		/* We have received measurement report, wakeup measurement wait queue */
		PRINTM(MINFO, "Measurement Report\n");
		/* Going out of CAC checking period */
		if (priv->phandle->cac_period == MTRUE) {
			priv->phandle->cac_period = MFALSE;
			if (priv->phandle->meas_wait_q_woken == MFALSE) {
				priv->phandle->meas_wait_q_woken = MTRUE;
				wake_up_interruptible(&priv->phandle->
						      meas_wait_q);
			}

			/* Execute delayed BSS START command */
			if (priv->phandle->delay_bss_start == MTRUE) {
				mlan_ioctl_req *req = NULL;
				mlan_ds_bss *bss = NULL;

				/* Clear flag */
				priv->phandle->delay_bss_start = MFALSE;

				PRINTM(MMSG,
				       "Now CAC measure period end. Execute delayed BSS Start command.\n");

				req = woal_alloc_mlan_ioctl_req(sizeof
								(mlan_ds_bss));
				if (!req) {
					PRINTM(MERROR,
					       "Failed to allocate ioctl request buffer\n");
					goto done;
				}
				bss = (mlan_ds_bss *)req->pbuf;
				req->req_id = MLAN_IOCTL_BSS;
				req->action = MLAN_ACT_SET;
				bss->sub_command = MLAN_OID_BSS_START;
				memcpy(&bss->param.ssid_bssid,
				       &priv->phandle->delay_ssid_bssid,
				       sizeof(mlan_ssid_bssid));

				if (woal_request_ioctl(priv, req, MOAL_NO_WAIT)
				    != MLAN_STATUS_PENDING) {
					PRINTM(MERROR,
					       "Delayed BSS Start operation failed!\n");
					kfree(req);
				}

				PRINTM(MMSG, "BSS START Complete!\n");
			}
#ifdef UAP_SUPPORT
#if defined(STA_CFG80211) || defined(UAP_CFG80211)
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
			if (priv->uap_host_based && dfs_offload)
				woal_cfg80211_dfs_vendor_event(priv,
							       event_dfs_cac_finished,
							       &priv->chan);
#endif
#endif
#endif

		}
		break;
	case MLAN_EVENT_ID_DRV_TDLS_TEARDOWN_REQ:
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
#ifdef STA_CFG80211
		if (IS_STA_CFG80211(cfg80211_wext)) {
			tdls_tear_down_event *tdls_event =
				(tdls_tear_down_event *)pmevent->event_buf;
			cfg80211_tdls_oper_request(priv->netdev,
						   tdls_event->peer_mac_addr,
						   NL80211_TDLS_TEARDOWN,
						   tdls_event->reason_code,
						   GFP_KERNEL);
		}
#endif
#endif
		break;
	case MLAN_EVENT_ID_FW_TX_STATUS:
		{
#if defined(STA_CFG80211) || defined(UAP_CFG80211)
			unsigned long flag;
			tx_status_event *tx_status =
				(tx_status_event *)(pmevent->event_buf + 4);
			struct tx_status_info *tx_info = NULL;

			PRINTM(MINFO,
			       "Receive Tx status: tx_token=%d, pkt_type=0x%x, status=%d tx_seq_num=%d\n",
			       tx_status->tx_token_id, tx_status->packet_type,
			       tx_status->status, priv->tx_seq_num);
			spin_lock_irqsave(&priv->tx_stat_lock, flag);
			tx_info =
				woal_get_tx_info(priv, tx_status->tx_token_id);
			if (tx_info) {
				bool ack;
				struct sk_buff *skb =
					(struct sk_buff *)tx_info->tx_skb;
				list_del(&tx_info->link);
				spin_unlock_irqrestore(&priv->tx_stat_lock,
						       flag);
				if (!tx_status->status)
					ack = true;
				else
					ack = false;
				if (priv->phandle->remain_on_channel &&
				    tx_info->cancel_remain_on_channel) {
					remain_priv =
						priv->phandle->priv[priv->
								    phandle->
								    remain_bss_index];
					if (remain_priv) {
						woal_cfg80211_remain_on_channel_cfg
							(remain_priv,
							 MOAL_NO_WAIT, MTRUE,
							 &channel_status, NULL,
							 0, 0);
						priv->phandle->
							remain_on_channel =
							MFALSE;
					}
				}
				PRINTM(MEVENT, "Wlan: Tx status=%d\n", ack);
#if defined(STA_CFG80211) || defined(UAP_CFG80211)
				if (tx_info->tx_cookie) {
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
#if CFG80211_VERSION_CODE < KERNEL_VERSION(3, 6, 0)
					cfg80211_mgmt_tx_status(priv->netdev,
								tx_info->
								tx_cookie,
								skb->data,
								skb->len, ack,
								GFP_ATOMIC);
#else
					cfg80211_mgmt_tx_status(priv->wdev,
								tx_info->
								tx_cookie,
								skb->data,
								skb->len, ack,
								GFP_ATOMIC);
#endif
#endif
				}
#endif
				dev_kfree_skb_any(skb);
				kfree(tx_info);
			} else
				spin_unlock_irqrestore(&priv->tx_stat_lock,
						       flag);
#endif
		}
		break;

	case MLAN_EVENT_ID_DRV_FT_RESPONSE:
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(3,10,0)
#ifdef STA_CFG80211
		if (IS_STA_CFG80211(cfg80211_wext)) {
			struct cfg80211_ft_event_params ft_event;
			if (priv->ft_pre_connect)
				break;
			memset(&ft_event, 0,
			       sizeof(struct cfg80211_ft_event_params));
			PRINTM(MMSG,
			       "wlan : FT response  target AP " MACSTR "\n",
			       MAC2STR((t_u8 *)pmevent->event_buf));
			DBG_HEXDUMP(MDAT_D, "FT-event ", pmevent->event_buf,
				    pmevent->event_len);
			memcpy(priv->target_ap_bssid, pmevent->event_buf,
			       ETH_ALEN);
			ft_event.target_ap = priv->target_ap_bssid;
			ft_event.ies = pmevent->event_buf + ETH_ALEN;
			ft_event.ies_len = pmevent->event_len - ETH_ALEN;
			/*TSPEC info is needed by RIC, However the TS operation is configured by mlanutl */
			/*So do not add RIC temporally */
			/*when add RIC, 1. query TS status, 2. copy tspec from addts command */
			ft_event.ric_ies = NULL;
			ft_event.ric_ies_len = 0;

			cfg80211_ft_event(priv->netdev, &ft_event);
			priv->ft_pre_connect = MTRUE;

			if (priv->ft_roaming_triggered_by_driver ||
			    !(priv->ft_cap & MBIT(0))) {
				priv->ft_wait_condition = MTRUE;
				wake_up(&priv->ft_wait_q);
			}
		}
#endif
#endif
		break;
	default:
		break;
	}
done:
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function prints the debug message in mlan
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param level    debug level
 *  @param pformat  point to string format buf
 *
 *  @return         N/A
 */
t_void
moal_print(IN t_void *pmoal_handle, IN t_u32 level, IN char *pformat, IN ...)
{
#ifdef	DEBUG_LEVEL1
	va_list args;

	if (level & MHEX_DUMP) {
		t_u8 *buf = NULL;
		int len = 0;

		va_start(args, pformat);
		buf = (t_u8 *)va_arg(args, t_u8 *);
		len = (int)va_arg(args, int);
		va_end(args);

#ifdef DEBUG_LEVEL2
		if (level & MINFO)
			HEXDUMP((char *)pformat, buf, len);
		else
#endif /* DEBUG_LEVEL2 */
		{
			if (level & MERROR)
				DBG_HEXDUMP(MERROR, (char *)pformat, buf, len);
			if (level & MCMD_D)
				DBG_HEXDUMP(MCMD_D, (char *)pformat, buf, len);
			if (level & MDAT_D)
				DBG_HEXDUMP(MDAT_D, (char *)pformat, buf, len);
			if (level & MIF_D)
				DBG_HEXDUMP(MIF_D, (char *)pformat, buf, len);
			if (level & MFW_D)
				DBG_HEXDUMP(MFW_D, (char *)pformat, buf, len);
			if (level & MEVT_D)
				DBG_HEXDUMP(MEVT_D, (char *)pformat, buf, len);
		}
	} else {
		if (drvdbg & level) {
			va_start(args, pformat);
			vprintk(pformat, args);
			va_end(args);
		}
	}
#endif /* DEBUG_LEVEL1 */
}

/**
 *  @brief This function prints the network interface name
 *
 *  @param pmoal_handle Pointer to the MOAL context
 *  @param bss_index    BSS index
 *  @param level        debug level
 *
 *  @return            N/A
 */
t_void
moal_print_netintf(IN t_void *pmoal_handle, IN t_u32 bss_index, IN t_u32 level)
{
#ifdef DEBUG_LEVEL1
	moal_handle *phandle = (moal_handle *)pmoal_handle;

	if (phandle) {
		if ((bss_index < MLAN_MAX_BSS_NUM) && phandle->priv[bss_index]
		    && phandle->priv[bss_index]->netdev) {
			if (drvdbg & level)
				printk("%s: ",
				       phandle->priv[bss_index]->netdev->name);
		}
	}
#endif /* DEBUG_LEVEL1 */
}

/**
 *  @brief This function asserts the existence of the passed argument
 *
 *  @param pmoal_handle     A pointer to moal_private structure
 *  @param cond             Condition to check
 *
 *  @return                 N/A
 */
t_void
moal_assert(IN t_void *pmoal_handle, IN t_u32 cond)
{
	if (!cond) {
		panic("Assert failed: Panic!");
	}
}

/**
 *  @brief This function save the histogram data
 *
 *  @param pmoal_handle     A pointer to moal_private structure
 *  @param bss_index        BSS index
 *  @param rx_rate          rx rate index
 *  @param snr              snr
 *  @param nflr             noise floor
 *  @param antenna          antenna
 *
 *  @return                 N/A
 */
t_void
moal_hist_data_add(IN t_void *pmoal_handle, IN t_u32 bss_index, IN t_u8 rx_rate,
		   IN t_s8 snr, IN t_s8 nflr, IN t_u8 antenna)
{
	moal_private *priv = NULL;
	priv = woal_bss_index_to_priv(pmoal_handle, bss_index);
	if (priv && antenna >= priv->phandle->histogram_table_num)
		antenna = 0;
	if (priv && priv->hist_data[antenna])
		woal_hist_data_add(priv, rx_rate, snr, nflr, antenna);
}

/**
 *  @brief This function update the peer signal
 *
 *  @param pmoal_handle     A pointer to moal_private structure
 *  @param bss_index        BSS index
 *  @param peer_addr        peer address
 *  @param snr              snr
 *  @param nflr             noise floor
 *
 *  @return                 N/A
 */
t_void
moal_updata_peer_signal(IN t_void *pmoal_handle, IN t_u32 bss_index,
			IN t_u8 *peer_addr, IN t_s8 snr, IN t_s8 nflr)
{
	moal_private *priv = NULL;
	struct tdls_peer *peer = NULL;
	unsigned long flags;
	priv = woal_bss_index_to_priv(pmoal_handle, bss_index);
	if (priv && priv->enable_auto_tdls) {
		spin_lock_irqsave(&priv->tdls_lock, flags);
		list_for_each_entry(peer, &priv->tdls_list, link) {
			if (!memcmp(peer->peer_addr, peer_addr, ETH_ALEN)) {
				peer->rssi = nflr - snr;
				peer->rssi_jiffies = jiffies;
				break;
			}
		}
		spin_unlock_irqrestore(&priv->tdls_lock, flags);
	}
}

/**
 *  @brief Performs division of 64-bit num with base
 *  @brief do_div does two things
 *  @brief 1. modifies the 64-bit num in place with
 *  @brief the quotient, i.e., num becomes quotient
 *  @brief 2. do_div() returns the 32-bit reminder
 *
 *  @param num   dividend
 *  @param base  divisor
 *  @return      returns 64-bit quotient
 */
t_u64
moal_do_div(IN t_u64 num, IN t_u32 base)
{
	t_u64 val = num;
	do_div(val, base);
	return val;
}
