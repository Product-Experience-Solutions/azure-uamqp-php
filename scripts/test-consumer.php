<?php

use Azure\uAMQP\Connection;
use Azure\uAMQP\Message;

require_once __DIR__ . '/emulator-parameters.php';

echo "Testing PHP uAMQP consumer against Service Bus emulator\n";
echo "========================================================\n\n";

try {
    echo "1. Creating connection...\n";
    $conn = new Connection(
        PHUAMQP_HOST,
        PHUAMQP_PORT,
        PHUAMQP_USE_TLS,
        PHUAMQP_KEY_NAME,
        PHUAMQP_ACCESS_KEY,
        false
    );
    echo "✓ Connection created\n\n";

    echo "2. Setting up callback for queue `" . PHUAMQP_QUEUE_NAME . "`...\n";
    $messageCount = 0;
    $receivedBodies = [];
    $deadline = time() + PHUAMQP_CONSUMER_TIMEOUT;

    $conn->setCallback(
        PHUAMQP_QUEUE_NAME,
        function (Message $message) use (&$messageCount, &$receivedBodies) {
            $body = $message->getBody();
            $receivedBodies[] = $body;
            $messageCount++;

            echo sprintf("✓ Message %d received: %s\n", $messageCount, $body);

            $payload = json_decode($body, true, 512, JSON_THROW_ON_ERROR);
            if (($payload['source'] ?? null) !== 'phuamqp-producer') {
                throw new RuntimeException('Unexpected message source in payload');
            }

            if (($payload['message'] ?? null) !== $messageCount) {
                throw new RuntimeException(sprintf('Unexpected message sequence. Expected %d, got %s', $messageCount, (string) ($payload['message'] ?? 'null')));
            }
        },
        function () use (&$conn, &$messageCount, $deadline) {
            echo "✓ Loop function started\n";
            while ($messageCount < PHUAMQP_MESSAGE_COUNT) {
                if (time() > $deadline) {
                    throw new RuntimeException(sprintf(
                        'Timed out waiting for %d messages; received %d',
                        PHUAMQP_MESSAGE_COUNT,
                        $messageCount
                    ));
                }

                try {
                    $conn->consume();
                } catch (\Throwable $e) {
                    echo "✗ ERROR in consume: " . $e->getMessage() . "\n";
                    throw $e;
                }

                usleep(100000);
            }
            echo "✓ Loop finished\n";
        }
    );

    if ($messageCount !== PHUAMQP_MESSAGE_COUNT) {
        throw new RuntimeException(sprintf(
            'Expected %d messages, received %d',
            PHUAMQP_MESSAGE_COUNT,
            $messageCount
        ));
    }

    $conn->close();
    echo "\n✓ Consumer test completed successfully\n";
} catch (\Throwable $e) {
    echo "\n✗ ERROR: " . $e->getMessage() . "\n";
    exit(1);
}

