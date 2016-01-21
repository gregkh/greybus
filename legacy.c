/*
 * Greybus legacy-protocol driver
 *
 * Copyright 2015 Google Inc.
 * Copyright 2015 Linaro Ltd.
 *
 * Released under the GPLv2 only.
 */

#include "greybus.h"
#include "legacy.h"
#include "protocol.h"


static int legacy_connection_get_version(struct gb_connection *connection)
{
	int ret;

	ret = gb_protocol_get_version(connection);
	if (ret) {
		dev_err(&connection->hd->dev,
			"%s: failed to get protocol version: %d\n",
			connection->name, ret);
		return ret;
	}

	return 0;
}

static int legacy_connection_bind_protocol(struct gb_connection *connection)
{
	struct gb_protocol *protocol;

	protocol = gb_protocol_get(connection->protocol_id,
				   connection->major,
				   connection->minor);
	if (!protocol) {
		dev_err(&connection->hd->dev,
				"protocol 0x%02x version %u.%u not found\n",
				connection->protocol_id,
				connection->major, connection->minor);
		return -EPROTONOSUPPORT;
	}
	connection->protocol = protocol;

	return 0;
}

static void legacy_connection_unbind_protocol(struct gb_connection *connection)
{
	struct gb_protocol *protocol = connection->protocol;

	gb_protocol_put(protocol);

	connection->protocol = NULL;
}

static int legacy_request_handler(struct gb_operation *operation)
{
	struct gb_protocol *protocol = operation->connection->protocol;

	return protocol->request_recv(operation->type, operation);
}

static int legacy_connection_init(struct gb_connection *connection)
{
	gb_request_handler_t handler;
	int ret;

	ret = legacy_connection_bind_protocol(connection);
	if (ret)
		return ret;

	if (connection->protocol->request_recv)
		handler = legacy_request_handler;
	else
		handler = NULL;

	ret = gb_connection_enable(connection, handler);
	if (ret)
		goto err_unbind_protocol;

	ret = legacy_connection_get_version(connection);
	if (ret)
		goto err_disable;

	ret = connection->protocol->connection_init(connection);
	if (ret)
		goto err_disable;

	return 0;

err_disable:
	gb_connection_disable(connection);
err_unbind_protocol:
	legacy_connection_unbind_protocol(connection);

	return ret;
}

static void legacy_connection_exit(struct gb_connection *connection)
{
	if (!connection->protocol)
		return;

	gb_connection_disable(connection);

	connection->protocol->connection_exit(connection);

	legacy_connection_unbind_protocol(connection);
}

static int legacy_probe(struct gb_bundle *bundle,
			const struct greybus_bundle_id *id)
{
	struct gb_connection *connection;
	int ret;

	dev_dbg(&bundle->dev, "%s - bundle class = 0x%02x\n", __func__,
			bundle->class);

	list_for_each_entry(connection, &bundle->connections, bundle_links) {
		dev_dbg(&bundle->dev, "enabling connection %s\n",
				connection->name);

		ret = legacy_connection_init(connection);
		if (ret)
			goto err_connections_disable;
	}

	return 0;

err_connections_disable:
	list_for_each_entry_reverse(connection, &bundle->connections,
							bundle_links) {
		legacy_connection_exit(connection);
	}

	return ret;
}

static void legacy_disconnect(struct gb_bundle *bundle)
{
	struct gb_connection *connection;

	dev_dbg(&bundle->dev, "%s - bundle class = 0x%02x\n", __func__,
			bundle->class);

	list_for_each_entry_reverse(connection, &bundle->connections,
							bundle_links) {
		legacy_connection_exit(connection);
	}
}

static const struct greybus_bundle_id legacy_id_table[] = {
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_GPIO) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_I2C) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_UART) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_HID) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_USB) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_SDIO) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_POWER_SUPPLY) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_PWM) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_SPI) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_DISPLAY) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_CAMERA) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_SENSOR) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_LIGHTS) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_VIBRATOR) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_LOOPBACK) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_AUDIO_MGMT) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_AUDIO_DATA) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_SVC) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_FIRMWARE) },
	{ GREYBUS_DEVICE_CLASS(GREYBUS_CLASS_RAW) },
	{ }
};
MODULE_DEVICE_TABLE(greybus, legacy_id_table);

static struct greybus_driver legacy_driver = {
	.name		= "legacy",
	.probe		= legacy_probe,
	.disconnect	= legacy_disconnect,
	.id_table	= legacy_id_table,
};

int gb_legacy_init(void)
{
	return greybus_register(&legacy_driver);
}

void gb_legacy_exit(void)
{
	greybus_deregister(&legacy_driver);
}
