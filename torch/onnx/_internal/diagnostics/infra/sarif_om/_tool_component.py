# DO NOT EDIT! This file was generated by jschema_to_python version 0.0.1.dev29,
# with extension for dataclasses and type annotation.

from __future__ import annotations

import dataclasses
from typing import Any


@dataclasses.dataclass
class ToolComponent(object):
    """A component, such as a plug-in or the driver, of the analysis tool that was run."""

    name: Any = dataclasses.field(metadata={"schema_property_name": "name"})
    associated_component: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "associatedComponent"}
    )
    contents: Any = dataclasses.field(
        default_factory=lambda: ["localizedData", "nonLocalizedData"],
        metadata={"schema_property_name": "contents"},
    )
    dotted_quad_file_version: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "dottedQuadFileVersion"}
    )
    download_uri: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "downloadUri"}
    )
    full_description: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "fullDescription"}
    )
    full_name: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "fullName"}
    )
    global_message_strings: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "globalMessageStrings"}
    )
    guid: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "guid"}
    )
    information_uri: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "informationUri"}
    )
    is_comprehensive: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "isComprehensive"}
    )
    language: Any = dataclasses.field(
        default="en-US", metadata={"schema_property_name": "language"}
    )
    localized_data_semantic_version: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "localizedDataSemanticVersion"}
    )
    locations: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "locations"}
    )
    minimum_required_localized_data_semantic_version: Any = dataclasses.field(
        default=None,
        metadata={
            "schema_property_name": "minimumRequiredLocalizedDataSemanticVersion"
        },
    )
    notifications: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "notifications"}
    )
    organization: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "organization"}
    )
    product: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "product"}
    )
    product_suite: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "productSuite"}
    )
    properties: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "properties"}
    )
    release_date_utc: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "releaseDateUtc"}
    )
    rules: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "rules"}
    )
    semantic_version: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "semanticVersion"}
    )
    short_description: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "shortDescription"}
    )
    supported_taxonomies: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "supportedTaxonomies"}
    )
    taxa: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "taxa"}
    )
    translation_metadata: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "translationMetadata"}
    )
    version: Any = dataclasses.field(
        default=None, metadata={"schema_property_name": "version"}
    )


# flake8: noqa
