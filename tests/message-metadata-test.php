<?php

declare(strict_types=1);

use Azure\uAMQP\Message;

function requireSame(mixed $expected, mixed $actual, string $description): void
{
    if ($expected !== $actual) {
        throw new RuntimeException($description . ': ' . var_export($actual, true));
    }
}

$message = new Message("body\0bytes");
requireSame("body\0bytes", $message->getBody(), 'Body bytes');
requireSame(null, $message->getMessageId(), 'Absent message ID');
requireSame(0, $message->getDeliveryCount(), 'Initial delivery count');
requireSame([], $message->getApplicationProperties(), 'Absent application properties');
requireSame([], $message->getMessageAnnotations(), 'Absent message annotations');
requireSame(null, $message->getApplicationProperty('absent'), 'Absent property');
requireSame(null, $message->getMessageAnnotation('absent', 'T'), 'Absent annotation');
$message->setMessageId('metadata-test-id');
requireSame('metadata-test-id', $message->getMessageId(), 'Message ID');
$message->setApplicationProperty('name', 'S', 'synthetic');
$message->setApplicationProperty('number', 'I', 42);
$message->setApplicationProperty('active', 'B', true);
$message->setApplicationProperty('long', 'L', 9000000000000);
$message->setApplicationProperty('double', 'D', 1.25);
requireSame('synthetic', $message->getApplicationProperty('name'), 'String property');
requireSame(42, $message->getApplicationProperty('number', 'I'), 'Legacy integer getter');
requireSame('42', $message->getApplicationProperty('number', 'S'), 'Legacy string conversion');
requireSame(true, $message->getApplicationProperty('active'), 'Boolean property');
$message->setApplicationProperty('active', 'B', false);
requireSame(false, $message->getApplicationProperty('active'), 'False boolean property');
requireSame(9000000000000, $message->getApplicationProperty('long'), 'Long property');
requireSame(1.25, $message->getApplicationProperty('double'), 'Double property');
requireSame(5, count($message->getApplicationProperties()), 'Application property map');
$message->setApplicationProperty('number', 'I', 43);
requireSame(43, $message->getApplicationProperty('number'), 'Overwritten property');
$message->setMessageAnnotation('x-opt-scheduled-enqueue-time', 'T', 1770000000123);
requireSame(1770000000123, $message->getMessageAnnotation('x-opt-scheduled-enqueue-time'), 'Timestamp annotation');
requireSame(['x-opt-scheduled-enqueue-time' => 1770000000123], $message->getMessageAnnotations(), 'Annotations map');
$copy = clone $message;
$message->setMessageId('changed-original-id');
$message->setApplicationProperty('number', 'I', 99);
$message->setMessageAnnotation('x-opt-scheduled-enqueue-time', 'T', 1770000000456);
unset($message);
requireSame('metadata-test-id', $copy->getMessageId(), 'Cloned message ID lifetime');
requireSame(43, $copy->getApplicationProperty('number'), 'Cloned property lifetime');
requireSame(1770000000123, $copy->getMessageAnnotation('x-opt-scheduled-enqueue-time'), 'Cloned annotation isolation');
requireSame("body\0bytes", $copy->getBody(), 'Cloned body lifetime');
unset($copy);
echo "PHP message metadata tests passed\n";
