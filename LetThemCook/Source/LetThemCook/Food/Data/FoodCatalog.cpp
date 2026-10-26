#include "FoodCatalog.h"
#include "Delivery/DeliveryBox.h"
#include "Engine/Texture2D.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const FFoodProductDefinition* UFoodCatalog::FindProduct(FName ProductId) const
{
    if (ProductId.IsNone()) return nullptr;

    return Products.FindByPredicate([ProductId](const FFoodProductDefinition& Product)
    {
        return Product.Id == ProductId;
    });
}

bool UFoodCatalog::GetProduct(FName ProductId, FFoodProductDefinition& Product) const
{
    const FFoodProductDefinition* FoundProduct = FindProduct(ProductId);
    Product = FoundProduct ? *FoundProduct : FFoodProductDefinition();
    return FoundProduct != nullptr;
}

#if WITH_EDITOR
EDataValidationResult UFoodCatalog::IsDataValid(FDataValidationContext& Context) const
{
    const EDataValidationResult ParentResult = Super::IsDataValid(Context);
    bool bValid = ParentResult != EDataValidationResult::Invalid;
    if (!FMath::IsFinite(BoxScale) || BoxScale <= 0)
    {
        Context.AddError(NSLOCTEXT("FoodCatalog", "InvalidScale", "Box Scale must be a finite number greater than zero."));
        bValid = false;
    }
    TSet<FName> ProductIds;

    for (int32 Index = 0; Index < Products.Num(); ++Index)
    {
        const FFoodProductDefinition& Product = Products[Index];
        if (Product.Id.IsNone() || ProductIds.Contains(Product.Id))
        {
            Context.AddError(FText::Format(
                NSLOCTEXT("FoodCatalog", "InvalidId", "Product {0} must have a non-empty, unique ID."),
                FText::AsNumber(Index + 1)));
            bValid = false;
        }
        ProductIds.Add(Product.Id);

        if (Product.Name.IsEmpty())
        {
            Context.AddError(FText::Format(
                NSLOCTEXT("FoodCatalog", "MissingName", "Product {0} must have a display name."),
                FText::FromName(Product.Id)));
            bValid = false;
        }

        if (Product.Price <= 0)
        {
            Context.AddError(FText::Format(
                NSLOCTEXT("FoodCatalog", "InvalidPrice", "Product {0} must have a price greater than zero."),
                FText::FromName(Product.Id)));
            bValid = false;
        }

        FVector BoxExtent;
        FVector MeshOffset;
        if (!ADeliveryBox::GetProductGeometry(Product, BoxScale, BoxExtent, MeshOffset))
        {
            Context.AddError(FText::Format(
                NSLOCTEXT("FoodCatalog", "InvalidBox", "Product {0} must have a box mesh, positive mesh scale and collision half-size of at least 1 cm."),
                FText::FromName(Product.Id)));
            bValid = false;
        }
    }

    return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
