static int dw_spi_transfer_one(struct spi_controller *master,
		struct spi_device *spi, struct spi_transfer *transfer)
{
	struct dw_spi *dws = spi_controller_get_devdata(master);
	struct dw_spi_cfg cfg = {
		.tmode = SPI_TMOD_TR,
		.dfs = transfer->bits_per_word,
		.freq = transfer->speed_hz,
	};
	int ret;

	int i;
	
	printk("%s-%d-----------ph20250624\n", __FUNCTION__, __LINE__);

    printk("Transfer start: len=%zu, tx_buf=%p, rx_buf=%p\n",
            transfer->len, transfer->tx_buf, transfer->rx_buf);
    
    /* 打印发送的命令（如果有） */
    if (transfer->tx_buf && transfer->len <= 4) {
        u8 *cmd = kmemdup(transfer->tx_buf, transfer->len, GFP_KERNEL);
        if (cmd) {
            printk("CMD: ");
            for (i = 0; i < transfer->len; i++)
                printk("%02x ", cmd[i]);
            printk("\n");
            kfree(cmd);
        }
    }

	dws->dma_mapped = 0;
	dws->n_bytes = DIV_ROUND_UP(transfer->bits_per_word, BITS_PER_BYTE);
	dws->tx = (void *)transfer->tx_buf;
	dws->tx_len = transfer->len / dws->n_bytes;
	dws->rx = transfer->rx_buf;
	dws->rx_len = dws->tx_len;

	/* Ensure the data above is visible for all CPUs */
	smp_mb();

	spi_enable_chip(dws, 0);

	dw_spi_update_config(dws, spi, &cfg);

	transfer->effective_speed_hz = dws->current_freq;

	/* Check if current transfer is a DMA transaction */
	if (master->can_dma && master->can_dma(master, spi, transfer))
		dws->dma_mapped = master->cur_msg_mapped;

	/* For poll mode just disable all interrupts */
	spi_mask_intr(dws, 0xff);

	if (dws->dma_mapped) {
		ret = dws->dma_ops->dma_setup(dws, transfer);
		if (ret)
			return ret;
	}

	spi_enable_chip(dws, 1);

	if (dws->dma_mapped)
		return dws->dma_ops->dma_transfer(dws, transfer);
	else if (dws->irq == IRQ_NOTCONNECTED)
		return dw_spi_poll_transfer(dws, transfer);

	dw_spi_irq_setup(dws);

	dev_dbg(&master->dev, "Transfer complete\n");

	return 1;
}
