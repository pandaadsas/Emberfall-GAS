// Copyright (c) 2026 panda | Emberfall 余烬陨落 | MIT License


#include "UI/UserWidget/DuraUserWidget.h"

void UDuraUserWidget::SetWidgetController(UObject* InWidgetController)
{
	WidgetController = InWidgetController;
	WidgetControllerSet();
}
