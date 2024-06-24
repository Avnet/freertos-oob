#include "qspi.h"

#include <xparameters.h>
#include "xil_printf.h"
#include "xil_types.h"
#include "xqspipsu_flash_config.h"

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#define QSPIPSU_DEVICE_ID		XPAR_XQSPIPSU_0_DEVICE_ID

/*
 * Number of flash pages to be written.
 */
#define PAGE_COUNT		32

/*
 * Max page size to initialize write and read buffer
 */
#define MAX_PAGE_SIZE 1024

/*
 * Flash address to which data is to be written.
 */
#define TEST_ADDRESS		0x000000


#define ENTER_4B	1
#define EXIT_4B		0

u8 ReadCmd;
u8 WriteCmd;
u8 StatusCmd;
u8 SectorEraseCmd;
u8 FSRFlag;

u8 TxBfrPtr;
u8 ReadBfrPtr[3];
u32 FlashMake;
u32 FCTIndex;	/* Flash configuration table index */

/*
 * The instances to support the device drivers are global such that they
 * are initialized to zero each time the program runs. They could be local
 * but should at least be static so they are zeroed.
 */
static XQspiPsu QspiPsuInstance;
static XQspiPsu_Msg FlashMsg[5];

/*
 * The following variable allows a test value to be added to the values that
 * are written to the Flash such that unique values can be generated to
 * guarantee the writes to the Flash were successful
 */
int Test = 1;

/*
 * The following variables are used to read and write to the flash and they
 * are global to avoid having large buffers on the stack
 * The buffer size accounts for maximum page size and maximum banks -
 * for each bank separate read will be performed leading to that many
 * (overhead+dummy) bytes
 */
#ifdef __ICCARM__
#pragma data_alignment = 32
u8 ReadBuffer[(PAGE_COUNT * MAX_PAGE_SIZE) + (DATA_OFFSET + DUMMY_SIZE)*8];
#else
u8 ReadBuffer[(PAGE_COUNT * MAX_PAGE_SIZE) + (DATA_OFFSET + DUMMY_SIZE)*8] __attribute__ ((aligned(64)));
#endif
u8 WriteBuffer[(PAGE_COUNT * MAX_PAGE_SIZE) + DATA_OFFSET];
u8 CmdBfr[8];

/*
 * The following constants specify the max amount of data and the size of the
 * the buffer required to hold the data and overhead to transfer the data to
 * and from the Flash. Initialized to single flash page size.
 */
u32 MaxData = PAGE_COUNT*256;

static int FlashRead(XQspiPsu *QspiPsuPtr, u32 Address, u32 ByteCount, u8 Command,
				u8 *WriteBfrPtr, u8 *ReadBfrPtr);
static int MultiDieRead(XQspiPsu *QspiPsuPtr, u32 Address, u32 ByteCount, u8 Command,
		 u8 *WriteBfrPtr, u8 *ReadBfrPtr);
static int FlashReadID(XQspiPsu *QspiPsuPtr);
static u32 GetRealAddr(XQspiPsu *QspiPsuPtr, u32 Address);
static int FlashEnterExit4BAddMode(XQspiPsu *QspiPsuPtr, unsigned int Enable);
static int FlashEnableQuadMode(XQspiPsu *QspiPsuPtr);

/*****************************************************************************/
/**
 *
 * This function performs read. DMA is the default setting.
 *
 * @param	QspiPsuPtr is a pointer to the QSPIPSU driver component to use.
 * @param	Address contains the address of the first sector which needs to
 *			be erased.
 * @param	ByteCount contains the total size to be erased.
 * @param	Command is the command used to read data from the flash.
 *		Supports normal, fast, dual and quad read commands.
 * @param	WriteBfrPtr is pointer to the write buffer which contains data to be
 *		transmitted
 * @param	ReadBfrPtr is pointer to the read buffer to which valid received data
 *		should be written
 *
 * @return	XST_SUCCESS if successful, else XST_FAILURE.
 *
 * @note	None.
 *
 ******************************************************************************/
static int FlashRead(XQspiPsu *QspiPsuPtr, u32 Address, u32 ByteCount, u8 Command,
				u8 *WriteBfrPtr, u8 *ReadBfrPtr)
{
	u32 RealAddr;
	u32 DiscardByteCnt;
	u32 FlashMsgCnt;
	int Status;

	/* Check die boundary conditions if required for any flash */
	if (Flash_Config_Table[FCTIndex].NumDie > 1) {

		Status = MultiDieRead(QspiPsuPtr, Address, ByteCount, Command,
				      WriteBfrPtr, ReadBfrPtr);
		if (Status != XST_SUCCESS)
			return XST_FAILURE;
	} else {
		/* For Dual Stacked, split and read for boundary crossing */
		/*
		 * Translate address based on type of connection
		 * If stacked assert the slave select based on address
		 */
		RealAddr = GetRealAddr(QspiPsuPtr, Address);

		WriteBfrPtr[COMMAND_OFFSET]   = Command;
		if (Flash_Config_Table[FCTIndex].FlashDeviceSize > SIXTEENMB) {
			WriteBfrPtr[ADDRESS_1_OFFSET] =
					(u8)((RealAddr & 0xFF000000) >> 24);
			WriteBfrPtr[ADDRESS_2_OFFSET] =
					(u8)((RealAddr & 0xFF0000) >> 16);
			WriteBfrPtr[ADDRESS_3_OFFSET] =
					(u8)((RealAddr & 0xFF00) >> 8);
			WriteBfrPtr[ADDRESS_4_OFFSET] =
					(u8)(RealAddr & 0xFF);
			DiscardByteCnt = 5;
		} else {
			WriteBfrPtr[ADDRESS_1_OFFSET] =
					(u8)((RealAddr & 0xFF0000) >> 16);
			WriteBfrPtr[ADDRESS_2_OFFSET] =
					(u8)((RealAddr & 0xFF00) >> 8);
			WriteBfrPtr[ADDRESS_3_OFFSET] =
					(u8)(RealAddr & 0xFF);
			DiscardByteCnt = 4;
		}

		FlashMsg[0].TxBfrPtr = WriteBfrPtr;
		FlashMsg[0].RxBfrPtr = NULL;
		FlashMsg[0].ByteCount = DiscardByteCnt;
		FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

		FlashMsgCnt = 1;

		/* It is recommended to have a separate entry for dummy */
		if (Command == FAST_READ_CMD || Command == DUAL_READ_CMD ||
		    Command == QUAD_READ_CMD || Command == FAST_READ_CMD_4B ||
		    Command == DUAL_READ_CMD_4B ||
		    Command == QUAD_READ_CMD_4B) {
			/* Update Dummy cycles as per flash specs for QUAD IO */

			/*
			 * It is recommended that Bus width value during dummy
			 * phase should be same as data phase
			 */
			if (Command == FAST_READ_CMD ||
			    Command == FAST_READ_CMD_4B)
				FlashMsg[1].BusWidth =
						XQSPIPSU_SELECT_MODE_SPI;

			if (Command == DUAL_READ_CMD ||
			    Command == DUAL_READ_CMD_4B)
				FlashMsg[1].BusWidth =
						XQSPIPSU_SELECT_MODE_DUALSPI;

			if (Command == QUAD_READ_CMD ||
			    Command == QUAD_READ_CMD_4B)
				FlashMsg[1].BusWidth =
						XQSPIPSU_SELECT_MODE_QUADSPI;

			FlashMsg[1].TxBfrPtr = NULL;
			FlashMsg[1].RxBfrPtr = NULL;
			FlashMsg[1].ByteCount = DUMMY_CLOCKS;
			FlashMsg[1].Flags = 0;

			FlashMsgCnt++;
		}

		if (Command == FAST_READ_CMD ||
		    Command == FAST_READ_CMD_4B)
			FlashMsg[FlashMsgCnt].BusWidth =
					XQSPIPSU_SELECT_MODE_SPI;

		if (Command == DUAL_READ_CMD ||
		    Command == DUAL_READ_CMD_4B)
			FlashMsg[FlashMsgCnt].BusWidth =
					XQSPIPSU_SELECT_MODE_DUALSPI;

		if (Command == QUAD_READ_CMD ||
		    Command == QUAD_READ_CMD_4B)
			FlashMsg[FlashMsgCnt].BusWidth =
					XQSPIPSU_SELECT_MODE_QUADSPI;

		FlashMsg[FlashMsgCnt].TxBfrPtr = NULL;
		FlashMsg[FlashMsgCnt].RxBfrPtr = ReadBfrPtr;
		FlashMsg[FlashMsgCnt].ByteCount = ByteCount;
		FlashMsg[FlashMsgCnt].Flags = XQSPIPSU_MSG_FLAG_RX;

		if (QspiPsuPtr->Config.ConnectionMode ==
				XQSPIPSU_CONNECTION_MODE_PARALLEL)
			FlashMsg[FlashMsgCnt].Flags |=
					XQSPIPSU_MSG_FLAG_STRIPE;

		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, FlashMsg,
						 FlashMsgCnt + 1);
		if (Status != XST_SUCCESS)
			return XST_FAILURE;
	}
	return XST_SUCCESS;
}

/*****************************************************************************/
/**
 *
 * This functions performs a read operation for multi die flash devices.
 *
 * @param	QspiPsuPtr is a pointer to the QSPIPSU driver component to use.
 * @param	Address contains the address of the first sector which needs to
 *			be erased.
 * @param	ByteCount contains the total size to be erased.
 * @param	Command is the command used to read data from the flash.
 *			Supports normal, fast, dual and quad read commands.
 * @param	WriteBfrPtr is pointer to the write buffer which contains data to be
 *			transmitted
 * @param	ReadBfrPtr is pointer to the read buffer to which valid received data
 *			should be written.
 *
 * @return	XST_SUCCESS if successful, else XST_FAILURE.
 *
 * @note	None.
 *
 ******************************************************************************/
static int MultiDieRead(XQspiPsu *QspiPsuPtr, u32 Address, u32 ByteCount, u8 Command,
		 u8 *WriteBfrPtr, u8 *ReadBfrPtr)
{
	u32 RealAddr;
	u32 DiscardByteCnt;
	u32 FlashMsgCnt;
	int Status;
	u32 cur_bank = 0;
	u32 nxt_bank = 0;
	u32 bank_size;
	u32 remain_len = ByteCount;
	u32 data_len;
	u32 transfer_len;
	u8 *ReadBuffer = ReadBfrPtr;

	/*
	 * Some flash devices like N25Q512 have multiple dies
	 * in it. Read operation in these devices is bounded
	 * by its die segment. In a continuous read, across
	 * multiple dies, when the last byte of the selected
	 * die segment is read, the next byte read is the
	 * first byte of the same die segment. This is Die
	 * cross over issue. So to handle this issue, split
	 * a read transaction, that spans across multiple
	 * banks, into one read per bank. Bank size is 16MB
	 * for single and dual stacked mode and 32MB for dual
	 * parallel mode.
	 */
	if (QspiPsuPtr->Config.ConnectionMode ==
			XQSPIPSU_CONNECTION_MODE_PARALLEL)

		bank_size = SIXTEENMB << 1;

	else if (QspiPsuPtr->Config.ConnectionMode ==
			XQSPIPSU_CONNECTION_MODE_SINGLE)

		bank_size = SIXTEENMB;

	while (remain_len) {
		cur_bank = Address / bank_size;
		nxt_bank = (Address + remain_len) / bank_size;

		if (cur_bank != nxt_bank) {
			transfer_len = (bank_size * (cur_bank  + 1)) - Address;
			if (remain_len < transfer_len)
				data_len = remain_len;
			else
				data_len = transfer_len;
		} else {
			data_len = remain_len;
		}
		/*
		 * Translate address based on type of connection
		 * If stacked assert the slave select based on address
		 */
		RealAddr = GetRealAddr(QspiPsuPtr, Address);

		WriteBfrPtr[COMMAND_OFFSET]   = Command;
		if (Flash_Config_Table[FCTIndex].FlashDeviceSize > SIXTEENMB) {
			WriteBfrPtr[ADDRESS_1_OFFSET] =
					(u8)((RealAddr & 0xFF000000) >> 24);
			WriteBfrPtr[ADDRESS_2_OFFSET] =
					(u8)((RealAddr & 0xFF0000) >> 16);
			WriteBfrPtr[ADDRESS_3_OFFSET] =
					(u8)((RealAddr & 0xFF00) >> 8);
			WriteBfrPtr[ADDRESS_4_OFFSET] =
					(u8)(RealAddr & 0xFF);
			DiscardByteCnt = 5;
		} else {
			WriteBfrPtr[ADDRESS_1_OFFSET] =
					(u8)((RealAddr & 0xFF0000) >> 16);
			WriteBfrPtr[ADDRESS_2_OFFSET] =
					(u8)((RealAddr & 0xFF00) >> 8);
			WriteBfrPtr[ADDRESS_3_OFFSET] =
					(u8)(RealAddr & 0xFF);
			DiscardByteCnt = 4;
		}

		FlashMsg[0].TxBfrPtr = WriteBfrPtr;
		FlashMsg[0].RxBfrPtr = NULL;
		FlashMsg[0].ByteCount = DiscardByteCnt;
		FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

		FlashMsgCnt = 1;

		/* It is recommended to have a separate entry for dummy */
		if (Command == FAST_READ_CMD || Command == DUAL_READ_CMD ||
		    Command == QUAD_READ_CMD || Command == FAST_READ_CMD_4B ||
		    Command == DUAL_READ_CMD_4B ||
		    Command == QUAD_READ_CMD_4B) {
			/* Update Dummy cycles as per flash specs for QUAD IO */

			/*
			 * It is recommended that Bus width value during dummy
			 * phase should be same as data phase
			 */
			if (Command == FAST_READ_CMD ||
			    Command == FAST_READ_CMD_4B)
				FlashMsg[1].BusWidth =
						XQSPIPSU_SELECT_MODE_SPI;

			if (Command == DUAL_READ_CMD ||
			    Command == DUAL_READ_CMD_4B)
				FlashMsg[1].BusWidth =
						XQSPIPSU_SELECT_MODE_DUALSPI;

			if (Command == QUAD_READ_CMD ||
			    Command == QUAD_READ_CMD_4B)
				FlashMsg[1].BusWidth =
						XQSPIPSU_SELECT_MODE_QUADSPI;

			FlashMsg[1].TxBfrPtr = NULL;
			FlashMsg[1].RxBfrPtr = NULL;
			FlashMsg[1].ByteCount = DUMMY_CLOCKS;
			FlashMsg[1].Flags = 0;

			FlashMsgCnt++;
		}

		if (Command == FAST_READ_CMD ||
		    Command == FAST_READ_CMD_4B)
			FlashMsg[FlashMsgCnt].BusWidth =
					XQSPIPSU_SELECT_MODE_SPI;

		if (Command == DUAL_READ_CMD ||
		    Command == DUAL_READ_CMD_4B)
			FlashMsg[FlashMsgCnt].BusWidth =
					XQSPIPSU_SELECT_MODE_DUALSPI;

		if (Command == QUAD_READ_CMD ||
		    Command == QUAD_READ_CMD_4B)
			FlashMsg[FlashMsgCnt].BusWidth =
					XQSPIPSU_SELECT_MODE_QUADSPI;

		FlashMsg[FlashMsgCnt].TxBfrPtr = NULL;
		FlashMsg[FlashMsgCnt].RxBfrPtr = ReadBuffer;
		FlashMsg[FlashMsgCnt].ByteCount = data_len;
		FlashMsg[FlashMsgCnt].Flags = XQSPIPSU_MSG_FLAG_RX;

		if (QspiPsuPtr->Config.ConnectionMode ==
				XQSPIPSU_CONNECTION_MODE_PARALLEL)
			FlashMsg[FlashMsgCnt].Flags |=
					XQSPIPSU_MSG_FLAG_STRIPE;

		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, FlashMsg,
						 FlashMsgCnt + 1);
		if (Status != XST_SUCCESS)
			return XST_FAILURE;

		ReadBuffer += data_len;
		Address += data_len;
		remain_len -= data_len;
	}

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
 *
 * Reads the flash ID and identifies the flash in FCT table.
 *
 * @param	QspiPsuPtr is a pointer to the QSPIPSU driver component to use.
 *
 * @return	XST_SUCCESS if successful, else XST_FAILURE.
 *
 * @note	None.
 *
 *****************************************************************************/
static int FlashReadID(XQspiPsu *QspiPsuPtr)
{
	int Status;
	u32 ReadId = 0;

	/*
	 * Read ID
	 */
	TxBfrPtr = READ_ID;
	FlashMsg[0].TxBfrPtr = &TxBfrPtr;
	FlashMsg[0].RxBfrPtr = NULL;
	FlashMsg[0].ByteCount = 1;
	FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
	FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

	FlashMsg[1].TxBfrPtr = NULL;
	FlashMsg[1].RxBfrPtr = ReadBfrPtr;
	FlashMsg[1].ByteCount = 3;
	FlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
	FlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_RX;

	Status = XQspiPsu_PolledTransfer(QspiPsuPtr, FlashMsg, 2);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	xil_printf("qspi: FlashID=0x%x 0x%x 0x%x\n\r", ReadBfrPtr[0], ReadBfrPtr[1],
		   ReadBfrPtr[2]);

	/* In case of dual, read both and ensure they are same make/size */

	/*
	 * Deduce flash make
	 */
	FlashMake = ReadBfrPtr[0];

	ReadId = ((ReadBfrPtr[0] << 16) | (ReadBfrPtr[1] << 8) | ReadBfrPtr[2]);
	/*
	 * Assign corresponding index in the Flash configuration table
	 */
	Status = CalculateFCTIndex(ReadId, &FCTIndex);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
 *
 * This functions translates the address based on the type of interconnection.
 * In case of stacked, this function asserts the corresponding slave select.
 *
 * @param	QspiPsuPtr is a pointer to the QSPIPSU driver component to use.
 * @param	Address which is to be accessed (for erase, write or read)
 *
 * @return	RealAddr is the translated address - for single it is unchanged;
 *		for stacked, the lower flash size is subtracted;
 *		for parallel the address is divided by 2.
 *
 * @note	In addition to get the actual address to work on flash this
 *		function also selects the CS and BUS based on the configuration
 *		detected.
 *
 ******************************************************************************/
static u32 GetRealAddr(XQspiPsu *QspiPsuPtr, u32 Address)
{
	u32 RealAddr;

	switch (QspiPsuPtr->Config.ConnectionMode) {
	case XQSPIPSU_CONNECTION_MODE_SINGLE:
		XQspiPsu_SelectFlash(QspiPsuPtr,
				XQSPIPSU_SELECT_FLASH_CS_LOWER,
				XQSPIPSU_SELECT_FLASH_BUS_LOWER);
		RealAddr = Address;
		break;
	case XQSPIPSU_CONNECTION_MODE_STACKED:
		/* Select lower or upper Flash based on sector address */
		if (Address & Flash_Config_Table[FCTIndex].FlashDeviceSize) {

			XQspiPsu_SelectFlash(QspiPsuPtr,
					XQSPIPSU_SELECT_FLASH_CS_UPPER,
					XQSPIPSU_SELECT_FLASH_BUS_LOWER);
			/*
			 * Subtract first flash size when accessing second flash
			 */
			RealAddr = Address &
				(~Flash_Config_Table[FCTIndex].FlashDeviceSize);
		} else {
			/*
			 * Set selection to L_PAGE
			 */
			XQspiPsu_SelectFlash(QspiPsuPtr,
					XQSPIPSU_SELECT_FLASH_CS_LOWER,
					XQSPIPSU_SELECT_FLASH_BUS_LOWER);

			RealAddr = Address;

		}
		break;
	case XQSPIPSU_CONNECTION_MODE_PARALLEL:
		/*
		 * The effective address in each flash is the actual
		 * address / 2
		 */
		XQspiPsu_SelectFlash(QspiPsuPtr,
				XQSPIPSU_SELECT_FLASH_CS_BOTH,
				XQSPIPSU_SELECT_FLASH_BUS_BOTH);
		RealAddr = Address / 2;
		break;
	default:
		/* RealAddr wont be assigned in this case; */
	break;

	}

	return(RealAddr);
}

/*****************************************************************************/
/**
 * @brief
 * This API enters the flash device into 4 bytes addressing mode.
 * As per the Micron and ISSI spec, before issuing the command to enter
 * into 4 byte addr mode, a write enable command is issued.
 * For Macronix and Winbond flash parts write
 * enable is not required.
 *
 * @param	QspiPsuPtr is a pointer to the QSPIPSU driver component to use.
 * @param	Enable is a either 1 or 0 if 1 then enters 4 byte if 0 exits.
 *
 * @return
 *		- XST_SUCCESS if successful.
 *		- XST_FAILURE if it fails.
 *
 *
 ******************************************************************************/
static int FlashEnterExit4BAddMode(XQspiPsu *QspiPsuPtr, unsigned int Enable)
{
	int Status;
	u8 WriteEnableCmd;
	u8 Cmd;
	u8 WriteDisableCmd;
	u8 ReadStatusCmd;
	u8 WriteBuffer[2] = {0};
	u8 FlashStatus[2] = {0};

	if (Enable) {
		Cmd = ENTER_4B_ADDR_MODE;
	} else {
		if (FlashMake == ISSI_ID_BYTE0)
			Cmd = EXIT_4B_ADDR_MODE_ISSI;
		else
			Cmd = EXIT_4B_ADDR_MODE;
	}

	switch (FlashMake) {
	case ISSI_ID_BYTE0:
	case MICRON_ID_BYTE0:
		WriteEnableCmd = WRITE_ENABLE_CMD;
		GetRealAddr(QspiPsuPtr, TEST_ADDRESS);
		/*
		 * Send the write enable command to the
		 * Flash so that it can be written to, this
		 * needs to be sent as a separate transfer before
		 * the write
		 */
		FlashMsg[0].TxBfrPtr = &WriteEnableCmd;
		FlashMsg[0].RxBfrPtr = NULL;
		FlashMsg[0].ByteCount = 1;
		FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, FlashMsg, 1);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
		break;

	case SPANSION_ID_BYTE0:

		if (Enable) {
			WriteBuffer[0] = BANK_REG_WR;
			WriteBuffer[1] = 1 << 7;
		} else {
			WriteBuffer[0] = BANK_REG_WR;
			WriteBuffer[1] = 0 << 7;
		}

		FlashMsg[0].TxBfrPtr = WriteBuffer;
		FlashMsg[0].RxBfrPtr = NULL;
		FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;
		FlashMsg[0].ByteCount = 2;

		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, FlashMsg, 1);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		return Status;

	default:
		/*
		 * For Macronix and Winbond flash parts
		 * Write enable command is not required.
		 */
		break;
	}

	GetRealAddr(QspiPsuPtr, TEST_ADDRESS);

	FlashMsg[0].TxBfrPtr = &Cmd;
	FlashMsg[0].RxBfrPtr = NULL;
	FlashMsg[0].ByteCount = 1;
	FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
	FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

	Status = XQspiPsu_PolledTransfer(QspiPsuPtr, FlashMsg, 1);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	while (1) {
		ReadStatusCmd = StatusCmd;

		FlashMsg[0].TxBfrPtr = &ReadStatusCmd;
		FlashMsg[0].RxBfrPtr = NULL;
		FlashMsg[0].ByteCount = 1;
		FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

		FlashMsg[1].TxBfrPtr = NULL;
		FlashMsg[1].RxBfrPtr = FlashStatus;
		FlashMsg[1].ByteCount = 2;
		FlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_RX;

		if (QspiPsuPtr->Config.ConnectionMode ==
				XQSPIPSU_CONNECTION_MODE_PARALLEL) {
			FlashMsg[1].Flags |= XQSPIPSU_MSG_FLAG_STRIPE;
		}

		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, FlashMsg, 2);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		if (QspiPsuPtr->Config.ConnectionMode ==
				XQSPIPSU_CONNECTION_MODE_PARALLEL) {
			if (FSRFlag) {
				FlashStatus[1] &= FlashStatus[0];
			} else {
				FlashStatus[1] |= FlashStatus[0];
			}
		}

		if (FSRFlag) {
			if ((FlashStatus[1] & 0x80) != 0) {
				break;
			}
		} else {
			if ((FlashStatus[1] & 0x01) == 0) {
				break;
			}
		}
	}

	switch (FlashMake) {
	case ISSI_ID_BYTE0:
	case MICRON_ID_BYTE0:
		WriteDisableCmd = WRITE_DISABLE_CMD;
		GetRealAddr(QspiPsuPtr, TEST_ADDRESS);
		/*
		 * Send the write enable command to the
		 * Flash so that it can be written to,
		 * this needs to be sent as a separate
		 * transfer before
		 * the write
		 */
		FlashMsg[0].TxBfrPtr = &WriteDisableCmd;
		FlashMsg[0].RxBfrPtr = NULL;
		FlashMsg[0].ByteCount = 1;
		FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, FlashMsg, 1);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
		break;

	default:
		/*
		 * For Macronix and Winbond flash parts
		 * Write disable command is not required.
		 */
		break;
	}

	return Status;
}

/*****************************************************************************/
/**
 * @brief
 * This API enables Quad mode for the flash parts which require to enable quad
 * mode before using Quad commands.
 * For S25FL-L series flash parts this is required as the default configuration
 * is x1/x2 mode.
 *
 * @param	QspiPsuPtr is a pointer to the QSPIPSU driver component to use.
 *
 * @return
 *		- XST_SUCCESS if successful.
 *		- XST_FAILURE if it fails.
 *
 *
 ******************************************************************************/
static int FlashEnableQuadMode(XQspiPsu *QspiPsuPtr)
{
	int Status;
	u8 WriteEnableCmd;
	u8 ReadStatusCmd;
	u8 FlashStatus[2];
	u8 StatusRegVal;
	u8 WriteBuffer[3] = {0};

	switch (FlashMake) {
	case SPANSION_ID_BYTE0:
		if (FCTIndex <= 2) {
			TxBfrPtr = READ_CONFIG_CMD;
			FlashMsg[0].TxBfrPtr = &TxBfrPtr;
			FlashMsg[0].RxBfrPtr = NULL;
			FlashMsg[0].ByteCount = 1;
			FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
			FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

			FlashMsg[1].TxBfrPtr = NULL;
			FlashMsg[1].RxBfrPtr = &WriteBuffer[2];
			FlashMsg[1].ByteCount = 1;
			FlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
			FlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_RX;

			Status = XQspiPsu_PolledTransfer(QspiPsuPtr,
					FlashMsg, 2);
			if (Status != XST_SUCCESS) {
				return XST_FAILURE;
			}

			WriteEnableCmd = VOLATILE_WRITE_ENABLE_CMD;
			/*
			 * Send the write enable command to the
			 * Flash so that it can be written to, this
			 * needs to be sent as a separate transfer before
			 * the write
			 */
			FlashMsg[0].TxBfrPtr = &WriteEnableCmd;
			FlashMsg[0].RxBfrPtr = NULL;
			FlashMsg[0].ByteCount = 1;
			FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
			FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

			Status = XQspiPsu_PolledTransfer(QspiPsuPtr,
					FlashMsg, 1);
			if (Status != XST_SUCCESS) {
				return XST_FAILURE;
			}

			GetRealAddr(QspiPsuPtr, TEST_ADDRESS);

			WriteBuffer[0] = WRITE_CONFIG_CMD;
			WriteBuffer[1] |= 0;
			WriteBuffer[2] |= 1 << 1;

			FlashMsg[0].TxBfrPtr = WriteBuffer;
			FlashMsg[0].RxBfrPtr = NULL;
			FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
			FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;
			FlashMsg[0].ByteCount = 3;

			Status = XQspiPsu_PolledTransfer(QspiPsuPtr,
					FlashMsg, 1);
			if (Status != XST_SUCCESS) {
				return XST_FAILURE;
			}

			TxBfrPtr = READ_CONFIG_CMD;
			FlashMsg[0].TxBfrPtr = &TxBfrPtr;
			FlashMsg[0].RxBfrPtr = NULL;
			FlashMsg[0].ByteCount = 1;
			FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
			FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

			FlashMsg[1].TxBfrPtr = NULL;
			FlashMsg[1].RxBfrPtr = ReadBfrPtr;
			FlashMsg[1].ByteCount = 1;
			FlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
			FlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_RX;

			Status = XQspiPsu_PolledTransfer(QspiPsuPtr,
					FlashMsg, 2);
			if (Status != XST_SUCCESS) {
				return XST_FAILURE;
			}

			if (ReadBfrPtr[0] & 0x02) {
				Status = XST_SUCCESS;
			} else {
				Status = XST_FAILURE;
			}
		}
		break;
	case ISSI_ID_BYTE0:
		/*
		 * Read Status register
		 */
		ReadStatusCmd = READ_STATUS_CMD;
		FlashMsg[0].TxBfrPtr = &ReadStatusCmd;
		FlashMsg[0].RxBfrPtr = NULL;
		FlashMsg[0].ByteCount = 1;
		FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;
		FlashMsg[1].TxBfrPtr = NULL;
		FlashMsg[1].RxBfrPtr = FlashStatus;
		FlashMsg[1].ByteCount = 2;
		FlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_RX;
		if (QspiPsuPtr->Config.ConnectionMode ==
						XQSPIPSU_CONNECTION_MODE_PARALLEL) {
				FlashMsg[1].Flags |= XQSPIPSU_MSG_FLAG_STRIPE;
		}
		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, FlashMsg, 2);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
		if (QspiPsuPtr->Config.ConnectionMode ==
					XQSPIPSU_CONNECTION_MODE_PARALLEL) {
			if (FSRFlag) {
				FlashStatus[1] &= FlashStatus[0];
			}else {
				FlashStatus[1] |= FlashStatus[0];
			}
		}
		/*
		 * Set Quad Enable Bit in the buffer
		 */
		StatusRegVal = FlashStatus[1];
		StatusRegVal |= 0x1 << QUAD_MODE_ENABLE_BIT;

		/*
		 * Write Enable
		 */
		WriteEnableCmd = WRITE_ENABLE_CMD;
		FlashMsg[0].TxBfrPtr = &WriteEnableCmd;
		FlashMsg[0].RxBfrPtr = NULL;
		FlashMsg[0].ByteCount = 1;
		FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;
		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, FlashMsg, 1);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
		/*
		 * Write Status register
		 */
		WriteBuffer[COMMAND_OFFSET] = WRITE_STATUS_CMD;
		FlashMsg[0].TxBfrPtr = WriteBuffer;
		FlashMsg[0].RxBfrPtr = NULL;
		FlashMsg[0].ByteCount = 1;
		FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

		FlashMsg[1].TxBfrPtr = &StatusRegVal;
		FlashMsg[1].RxBfrPtr = NULL;
		FlashMsg[1].ByteCount = 1;
		FlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_TX;
		if (QspiPsuPtr->Config.ConnectionMode ==
				XQSPIPSU_CONNECTION_MODE_PARALLEL) {
			FlashMsg[1].Flags |= XQSPIPSU_MSG_FLAG_STRIPE;
		}
		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, FlashMsg, 2);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
		/*
		 * Write Disable
		 */
		WriteEnableCmd = WRITE_DISABLE_CMD;
		FlashMsg[0].TxBfrPtr = &WriteEnableCmd;
		FlashMsg[0].RxBfrPtr = NULL;
		FlashMsg[0].ByteCount = 1;
		FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;
		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, FlashMsg, 2);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
		break;

	case WINBOND_ID_BYTE0:
		ReadStatusCmd = READ_STATUS_REG_2_CMD;
		FlashMsg[0].TxBfrPtr = &ReadStatusCmd;
		FlashMsg[0].RxBfrPtr = NULL;
		FlashMsg[0].ByteCount = 1;
		FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;
		FlashMsg[1].TxBfrPtr = NULL;
		FlashMsg[1].RxBfrPtr = FlashStatus;
		FlashMsg[1].ByteCount = 2;
		FlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_RX;

		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, FlashMsg, 2);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}


		if (QspiPsuPtr->Config.ConnectionMode ==
			XQSPIPSU_CONNECTION_MODE_PARALLEL) {
			if (FSRFlag) {
				FlashStatus[1] &= FlashStatus[0];
			} else {
				FlashStatus[1] |= FlashStatus[0];
			}
		}
		/*
		 * Set Quad Enable Bit in the buffer
		 */
		StatusRegVal = FlashStatus[1];

		StatusRegVal |= 0x1 << WB_QUAD_MODE_ENABLE_BIT;

		/*
		 * Write Enable
		 */
		WriteEnableCmd = WRITE_ENABLE_CMD;
		FlashMsg[0].TxBfrPtr = &WriteEnableCmd;
		FlashMsg[0].RxBfrPtr = NULL;
		FlashMsg[0].ByteCount = 1;
		FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, FlashMsg, 1);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		/*
		 * Write Status register
		 */
		WriteBuffer[COMMAND_OFFSET] = WRITE_STATUS_REG_2_CMD;
		FlashMsg[0].TxBfrPtr = WriteBuffer;
		FlashMsg[0].RxBfrPtr = NULL;
		FlashMsg[0].ByteCount = 1;
		FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

		FlashMsg[1].TxBfrPtr = &StatusRegVal;
		FlashMsg[1].RxBfrPtr = NULL;
		FlashMsg[1].ByteCount = 1;
		FlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_TX;

		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, FlashMsg, 2);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
		while(1) {
			ReadStatusCmd = READ_STATUS_CMD;
			FlashMsg[0].TxBfrPtr = &ReadStatusCmd;
			FlashMsg[0].RxBfrPtr = NULL;
			FlashMsg[0].ByteCount = 1;
			FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
			FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;
			FlashMsg[1].TxBfrPtr = NULL;
			FlashMsg[1].RxBfrPtr = FlashStatus;
			FlashMsg[1].ByteCount = 2;
			FlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
			FlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_RX;

			Status = XQspiPsu_PolledTransfer(QspiPsuPtr, FlashMsg, 2);
			if (Status != XST_SUCCESS) {
				return XST_FAILURE;
			}


			if (QspiPsuPtr->Config.ConnectionMode ==
				XQSPIPSU_CONNECTION_MODE_PARALLEL) {
				if (FSRFlag) {
					FlashStatus[1] &= FlashStatus[0];
				} else {
					FlashStatus[1] |= FlashStatus[0];
				}
			}
			if ((FlashStatus[1] & 0x01) == 0x00) {
				break;
			}
		}
		/*
		 * Write Disable
		 */
		WriteEnableCmd = WRITE_DISABLE_CMD;
		FlashMsg[0].TxBfrPtr = &WriteEnableCmd;
		FlashMsg[0].RxBfrPtr = NULL;
		FlashMsg[0].ByteCount = 1;
		FlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		FlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;
		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, FlashMsg, 2);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

	default:
		/*
		 * Currently only S25FL-L series requires the
		 * Quad enable bit to be set to 1.
		 */
		Status = XST_SUCCESS;
		break;
	}

	return Status;
}

int32_t qspi_setup(void)
{
	int Status;
	XQspiPsu_Config *QspiPsuConfig;
	u32 PageSize = 0;

	/*
	 * Initialize the QSPIPSU driver so that it's ready to use
	 */
	QspiPsuConfig = XQspiPsu_LookupConfig(QSPIPSU_DEVICE_ID);
	if (QspiPsuConfig == NULL) {
		return XST_FAILURE;
	}

	Status = XQspiPsu_CfgInitialize(&QspiPsuInstance, QspiPsuConfig,
					QspiPsuConfig->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Set Manual Start
	 */
	XQspiPsu_SetOptions(&QspiPsuInstance, XQSPIPSU_MANUAL_START_OPTION);

	/*
	 * Set the prescaler for QSPIPSU clock
	 */
	XQspiPsu_SetClkPrescaler(&QspiPsuInstance, XQSPIPSU_CLK_PRESCALE_8);

	XQspiPsu_SelectFlash(&QspiPsuInstance,
		XQSPIPSU_SELECT_FLASH_CS_LOWER,
		XQSPIPSU_SELECT_FLASH_BUS_LOWER);

	/*
	 * Read flash ID and obtain all flash related information
	 * It is important to call the read id function before
	 * performing proceeding to any operation, including
	 * preparing the WriteBuffer
	 */
	Status = FlashReadID(&QspiPsuInstance);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

//	xil_printf("Flash connection mode : %d\n\r",
//			QspiPsuConfig->ConnectionMode);
//	xil_printf("where 0 - Single; 1 - Stacked; 2 - Parallel\n\r");
//	xil_printf("FCTIndex: %d\n\r", FCTIndex);

	/*
	 * Initialize MaxData according to page size.
	 */
	if(QspiPsuInstance.Config.ConnectionMode == XQSPIPSU_CONNECTION_MODE_PARALLEL)
		PageSize = Flash_Config_Table[FCTIndex].PageSize * 2;
	else
		PageSize = Flash_Config_Table[FCTIndex].PageSize;

	MaxData = PAGE_COUNT * PageSize;

	/*
	 * Some flash needs to enable Quad mode before using
	 * quad commands.
	 */
	Status = FlashEnableQuadMode(&QspiPsuInstance);
	if (Status != XST_SUCCESS)
		return XST_FAILURE;

	/*
	 * Address size and read command selection
	 * Micron flash on REMUS doesn't support this 4B write/erase cmd
	 */
	if(QspiPsuInstance.Config.BusWidth == BUSWIDTH_SINGLE)
		ReadCmd = FAST_READ_CMD;
	else if(QspiPsuInstance.Config.BusWidth == BUSWIDTH_DOUBLE)
		ReadCmd = DUAL_READ_CMD;
	else
		ReadCmd = QUAD_READ_CMD;

	WriteCmd = WRITE_CMD;
	SectorEraseCmd = SEC_ERASE_CMD;

	/* Status cmd - SR or FSR selection */
	if ((Flash_Config_Table[FCTIndex].NumDie > 1) &&
			(FlashMake == MICRON_ID_BYTE0)) {
		StatusCmd = READ_FLAG_STATUS_CMD;
		FSRFlag = 1;
	} else {
		StatusCmd = READ_STATUS_CMD;
		FSRFlag = 0;
	}

//	xil_printf("ReadCmd: 0x%x, WriteCmd: 0x%x,"
//		   " StatusCmd: 0x%x, FSRFlag: %d\n\r",
//		ReadCmd, WriteCmd, StatusCmd, FSRFlag);

	if (Flash_Config_Table[FCTIndex].FlashDeviceSize > SIXTEENMB) {
		Status = FlashEnterExit4BAddMode(&QspiPsuInstance, ENTER_4B);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
	}

	xil_printf("qspi: Setup Complete\n\r");

	return XST_SUCCESS;
}

int32_t qspi_retrieve_logfile(uint8_t *ReadBfrPtr, uint32_t *logfile_size)
{
	int Status;

	Status = FlashRead(&QspiPsuInstance, QSPI_USER_PART_OFFSET, 4, ReadCmd,
				CmdBfr, (u8 *) logfile_size);
	if (Status != XST_SUCCESS)
	{
		xil_printf("qspi: Error: Failed to read logfile size\r\n");
		return XST_FAILURE;
	}

	if (*logfile_size == 0 || *logfile_size > MAX_LOGFILE_SIZE)
	{
		xil_printf("qspi: Error: Incorrect logfile size (%d bytes)\r\n", *logfile_size);
		return XST_FAILURE;
	}

	Status = FlashRead(&QspiPsuInstance, QSPI_USER_PART_OFFSET + 4, *logfile_size, ReadCmd,
				CmdBfr, ReadBfrPtr);
	if (Status != XST_SUCCESS)
	{
		xil_printf("qspi: Error: Failed to read logfile\r\n");
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}

